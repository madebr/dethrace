#!/usr/bin/env python3

#######################################################
# Parses Watcom "exedump" output into structured data
# exedump: https://github.com/jeff-1amstudios/open-watcom-v2/tree/master/bld/exedump
#
# Usage: dumpparser.py <path to dump file>
#######################################################
from enum import Enum
import logging
from pathlib import Path
import re
from typing import IO
from typing import Iterable
from typing import Optional
from typing import Sequence
from typing import Union


logger = logging.getLogger(__name__)


class FileReader:
    __slots__ = ("_f", "linenb", "_next_line", )

    def __init__(self, io: IO):
        self._f = io
        self.linenb = 0
        self._next_line = None

    def eof(self):
        return self._peek_raw_line() == ""

    def _peek_raw_line(self):
        if self._next_line is None:
            self._next_line = self._f.readline()
        return self._next_line

    def peek_line(self):
        return self._peek_raw_line().strip()

    def read_line(self):
        if self._next_line is not None:
            self._next_line, line = None, self._next_line
            return line.strip()
        self.linenb += 1
        return self._f.readline().strip()


class DumpedModule:
    __slots__ = ("raw_data", )

    def __init__(self, data):
        self.raw_data = data

    @property
    def name(self):
        return self.raw_data["name"]

    def _remove_single_void_arguments(self):
        # if theres a single "void" argument for a function, we just remove it
        for fn in self.raw_data["functions"]:
            type_idx = fn["type"]
            fn_type = self.raw_data["types"][type_idx]
            if len(fn_type["args"]) == 1:
                arg_type = self.create_dumped_type(fn_type["args"][0])
                if isinstance(arg_type, DumpedVoidType):
                    fn_type["args"] = []

    def __repr__(self):
        return f"<{type(self).__name__}:{self.raw_data['name']}>"

    def iter_functions(self) -> Iterable["DumpedFunction"]:
        for fn_i in range(len(self.raw_data["functions"])):
            yield DumpedFunction(self, fn_i)

    def iter_globals(self) -> Iterable["DumpedGlobalVariable"]:
        for raw_global_i, raw_global in enumerate(self.raw_data["global_vars"]):
            dumped_type = self.create_dumped_type(raw_global["type"])
            yield DumpedGlobalVariable(self, raw_global_i, dumped_type)

    def create_dumped_type(self, type_idx: int):
        type_data = self.raw_data["types"][type_idx]
        if "base_type" in type_data:
            sub_dumped_type_index = type_data["base_type"]
            sub_dumped_type = self.create_dumped_type(sub_dumped_type_index)
            if type_data["type"] in ("NEAR386 PTR", "FAR386 PTR"):
                return DumpedPointerType(self, type_idx, sub_dumped_type)
            elif type_data["type"] in ("WORD_INDEX ARRAY", "BYTE_INDEX ARRAY"):
                return DumpedArrayType(self, type_idx, sub_dumped_type, type_data["upper_bound"] + 1)
            else:
                assert False

        elif type_data["type"] in ("NEAR386 PROC", "FAR386 PROC"):
            return DumpedFunctionType(self, type_idx)

        elif type_data["type"] == "FIELD_LIST":
            tag_index = None
            for ot_index, ot_data in self.raw_data["types"].items():
                if "type_idx" in ot_data and ot_data["type_idx"] == type_idx and ot_data["scope_idx"] != 0 and self.raw_data["types"][ot_data["scope_idx"]] in ("union", "struct"):
                    tag_index = ot_index
                    break
            if len(type_data["fields"]) > 1 and all(field["offset"] == 0 for field in type_data["fields"]):
                return DumpedUnionType(self, type_idx, tag_index)
            else:
                return DumpedStructType(self, type_idx, tag_index)

        elif type_data["type"] == "ENUM_LIST":
            tag_index = None
            for ot_index, ot_data in self.raw_data["types"].items():
                if "type_idx" in ot_data and ot_data["type_idx"] == type_idx and ot_data["scope_idx"] != 0 and self.raw_data["types"][ot_data["scope_idx"]] == "enum":
                    tag_index = ot_index
                    break
            return DumpedEnumType(self, type_idx, tag_index)

        elif type_data["type"] == "NAME":
            if type_data["scope_idx"] == 0:
                return DumpedTypedefType(self, type_idx)
            else:
                match self.raw_data["types"][type_data["scope_idx"]]["value"]:
                    case "struct":
                        return DumpedStructType(self, type_data["type_idx"], type_idx)
                    case "union":
                        return DumpedUnionType(self, type_data["type_idx"], type_idx)
                    case "enum":
                        return DumpedEnumType(self, type_data["type_idx"], type_idx)
                    case _:
                        raise ValueError(f"Unknown scope index: {type_data['scope_idx']}")

        elif type_data["type"] == "SCALAR":
            if type_data["value"] == "void":
                return DumpedVoidType(self, type_idx)
            return DumpedNamedType(self, type_idx)

        elif type_data["type"] == "SCOPE":
            return None

        raise NotImplementedError(f"Unknown type: { type_data }")


class DumpedType:
    __slots__ = ( "_module", "index", )
    def __init__(self, module: DumpedModule, index: int):
        self._module = module
        self.index = index

    @property
    def raw_data(self):
        return self._module.raw_data["types"][self.index]

class DumpedFunctionType(DumpedType):
    __slots__ = ( )

    def arguments(self):
        result = []
        raw_fn_type = self.raw_data
        for arg_i, arg_type_idx in enumerate(raw_fn_type["args"]):
            result.append(self._module.create_dumped_type(arg_type_idx))
        return result

    def return_type(self):
        return self._module.create_dumped_type(self.raw_data["return_type"])

    def short_repr(self):
        return f"(function (ret {self.return_type().short_repr()}) (args {' '.join(a.short_repr() for a in self.arguments())}))"


class DumpedFunction:
    __slots__ = ("_module", "index")
    def __init__(self, module: DumpedModule, index: int):
        self._module = module
        self.index = index

    @property
    def module(self):
        return self._module

    @property
    def raw_data(self):
        return self._module.raw_data["functions"][self.index]

    @property
    def type_index(self):
        return self.raw_data["type"]

    @property
    def raw_type_data(self):
        return self._module.raw_data["types"][self.type_index]

    @property
    def name(self):
        return self.raw_data["name"]

    @property
    def function_type(self):
        return DumpedFunctionType(self._module, self.type_index)

    def arguments(self):
        result = []
        for argument_type, local_var_data in zip(self.function_type.arguments(), self.raw_data["local_vars"]):
            # FIXME: create function to test for equivalence, ignoring pointer indices
            # assert local_var_data["type"] == argument_type.index
            result.append(DumpedTypedVariable(argument_type, local_var_data["name"]))
        return result

    def is_vararg(self) -> bool:
        return any(lv for lv in self.local_variables() if getattr(lv.kind, "name", None) == "va_list")

    def local_variables(self):
        result = []
        raw_type_data = self.raw_type_data
        argument_count = len(raw_type_data["args"])
        for raw_local_var in self.raw_data["local_vars"][argument_count:]:
            is_static = raw_local_var["addr_type"] == "CONST_ADDR386"
            result.append(DumpedLocalVariable(self._module.create_dumped_type(raw_local_var["type"]), raw_local_var["name"], static=is_static))
        return result

    def block_scopes(self):
        result = []
        raw_type_data = self.raw_type_data
        for raw_block_scope in self.raw_data["block_scopes"]:
            block_variables = []
            result.append(block_variables)
            for raw_block_variable in raw_block_scope:
                is_static = raw_block_variable["addr_type"] == "CONST_ADDR386"
                block_variables.append(DumpedLocalVariable(self._module.create_dumped_type(raw_block_variable["type"]), raw_block_variable["name"], static=is_static))
        return result

    def return_type(self):
        return self.function_type.return_type()


class DumpedNamedType(DumpedType):
    __slots__ = ( )

    @property
    def name(self):
        return self.raw_data["value"]

    def short_repr(self):
        return self.name


class DumpedTypedefType(DumpedNamedType):
    __slots__ = ( )

    @property
    def name(self):
        return self.raw_data["value"]

    def short_repr(self):
        return f"(typedef {self.name} {self.subtype()})"

    def subtype(self) -> DumpedType:
        try:
            return self._module.create_dumped_type(self.raw_data["type_idx"])
        except KeyError:
            raise


class DumpedPointerType(DumpedType):
    __slots__ = ("ptr", )
    def __init__(self, module: DumpedModule, index: int, ptr: DumpedType):
        super().__init__(module, index)
        self.ptr = ptr

    def short_repr(self):
        return f"(ptr {self.ptr})"


class DumpedArrayType(DumpedType):
    __slots__ = ("ptr", "count", )
    def __init__(self, module: DumpedModule, type_idx: int, ptr: DumpedType, count: int):
        super().__init__(module, type_idx)
        self.ptr = ptr
        self.count = count

    def short_repr(self):
        return f"(array {self.count} {self.ptr.short_repr()})"


class DumpedVoidType(DumpedType):
    __slots__ = ( )
    def short_repr(self):
        return "void"


class DumpedStructType(DumpedType):
    __slots__ = ( "tag_index", )

    def __init__(self, module: DumpedModule, index: int, tag_index: Optional[int]):
        super().__init__(module, index)
        self.tag_index = tag_index

    @property
    def members(self) -> list["DumpedTypedMemberVariable"]:
        members = []
        for raw_field in self.raw_data["fields"]:
            member_type = self._module.create_dumped_type(raw_field["type_idx"])
            members.append(DumpedTypedMemberVariable(member_type, raw_field["name"], raw_field["offset"],
                                                     start_bit=raw_field.get("start_bit"),
                                                     bit_size=raw_field.get("bit_size")))
        members.sort(key=lambda m: m.float_offset)
        return members

    @property
    def size(self):
        return self.raw_data["size"]

    @property
    def tag_name(self) -> Optional[str]:
        if self.tag_index is None:
            return None
        return self._module.raw_data["types"][self.tag_index]["value"]

    def short_repr(self):
        return f"(struct {' '.join(m.short_repr() for m in self.members)})"


class DumpedUnionType(DumpedType):
    __slots__ = ( "tag_index", )

    def __init__(self, module: DumpedModule, index: int, tag_index: Optional[int]):
        super().__init__(module, index)
        self.tag_index = tag_index

    @property
    def members(self) -> list["DumpedTypedMemberVariable"]:
        members = []
        for raw_field in self.raw_data["fields"]:
            member_type = self._module.create_dumped_type(raw_field["type_idx"])
            members.append(DumpedTypedMemberVariable(member_type, raw_field["name"], raw_field["offset"],
                                                     start_bit=raw_field.get("start_bit"), bit_size=raw_field.get("bit_size")))
        members.sort(key=lambda m: m.name)
        return members

    @property
    def size(self):
        return self.raw_data["size"]

    @property
    def tag_name(self) -> Optional[str]:
        if self.tag_index is None:
            return None
        return self._module.raw_data["types"][self.tag_index]["value"]

    def short_repr(self):
        return f"(union {' '.join(m.short_repr() for m in self.members)})"


class DumpedEnumType(DumpedType):
    __slots__ = ( "tag_index", )

    def __init__(self, module: DumpedModule, index: int, tag_index: Optional[int]):
        super().__init__(module, index)
        self.tag_index = tag_index

    @property
    def members(self) -> list[tuple[str, int]]:
        return sorted(dict(self.raw_data["elements"]).items(), key = lambda v: v[1])

    @property
    def tag_name(self) -> Optional[str]:
        if self.tag_index is None:
            return None
        return self._module.raw_data["types"][self.tag_index]["value"]

    def short_repr(self):
        return f"(enum {' '.join(f'({k} {v})' for k, v in self.raw_data['elements'].items())})"


class DumpedTypedVariable:
    __slots__ = ("kind", "name", )
    def __init__(self, kind: DumpedType, name: Optional[str]):
        self.kind = kind
        self.name = name

    def short_repr(self):
        return f"(variable {self.kind.short_repr()} {self.name})"


class DumpedTypedMemberVariable(DumpedTypedVariable):
    def __init__(self, kind: DumpedType, name: Optional[str], offset: int, start_bit: Optional[int], bit_size: Optional[int]):
        super().__init__(kind, name)
        self.offset = offset
        self.start_bit = start_bit
        self.bit_size = bit_size

    @property
    def float_offset(self) -> float:
        if self.start_bit is None:
            return float(self.offset)
        else:
            return self.offset + self.start_bit / 8

    def short_repr(self):
        return f"(member {self.kind.short_repr()} {self.name})"


class DumpedGlobalVariable(DumpedTypedVariable):
    def __init__(self, module: DumpedModule, index: int, kind: DumpedType):
        name = module.raw_data["global_vars"][index]["name"]
        super().__init__(kind, name)
        self.module = module
        self.index = index

    @property
    def raw_data(self):
        return self.module.raw_data["global_vars"][self.index]

    @property
    def address(self) -> int:
        return self.raw_data["addr"]["offset"]


class DumpedLocalVariable(DumpedTypedVariable):
    def __init__(self, kind: DumpedType, name: str, static: bool):
        super().__init__(kind, name)
        self.static = static

    def short_repr(self):
        return f"(localvariable {self.kind.short_repr()} {self.name} {self.static})"


class DumpedModuleReader:
    class _ReadState(Enum):
        NONE = 0
        MODULE = 1
        LOCALS = 2
        TYPES = 3

    class _LocalVariableScope(Enum):
        FUNCTION = 0
        BLOCK = 1
        ERROR = -1

    MODULE_START_REGEX = re.compile(r"\d+\) Name:\s+(\S+)")

    LOCALS_SECTION_HEADER = '*** Locals ***'
    LOCAL_HEADER_REGEX = re.compile(r"(\S+):\s+(\S+)")
    GLOBAL_VAR_REGEX = re.compile(r'"(.*)" addr = (.*),\s+type = (\d+)')
    FUNCTION_OFFSET_REGEX = re.compile(r"start off = (\S+), code size = (\S+), parent off = (\S+)")
    LOCAL_VAR_REGEX = re.compile(r'name = "(\S+)",  type = (\d+)')
    LOCAL_VAR_ADDR_REGEX = re.compile(r'address: (\S+)\( (\S+) \)')

    TYPES_SECTION_HEADER = "*** Types ***"
    TYPE_HEADER_REGEX = re.compile(r"(\S+): (.+)\((\d+)\)")
    ARRAY_TYPE_REGEX = re.compile(r'high bound = (\S+)   base type idx = (\d+)')
    NAME_TYPE_REGEX = re.compile(r'"(\S+)"  type idx = (\d+)  scope idx = (\d+)')
    FIELD_LIST_REGEX = re.compile(r'number of fields = (\S+)   size = (\S+)')

    FIELD_HEADER_REGEX = re.compile(r'(\S+):\s+(\S+)')
    FIELD_ITEM_REGEX = re.compile(r'"(\S+)"  offset = (\S+)  type idx = (\d+)')
    FIELD_ITEM_BIT_REGEX = re.compile(r'start bit = (\S+)  bit size = (\S+)')

    ENUM_LIST_REGEX = re.compile(r'number of consts = (\S+)   scalar type = (\S+), (\S+)')
    ENUM_ITEM_REGEX = re.compile(r'"(\S+)"   value = (\S+)')

    SECTION_UNDERLINE = re.compile(r'^(=*)$') # ignore these lines

    def __init__(self, io: IO):
        self._state = self._ReadState.NONE
        # list of blocks each containing a list of vars scoped to that block. Always local to a function.

        self._module_count = 0

        self._reader = FileReader(io)

    @classmethod
    def open(cls, path: Union[str, Path]) -> "DumpedModuleReader":
        return cls(open(path))

    def read_modules(self) -> Iterable["DumpedModule"]:
        current_module: Optional[DumpedModule] = None
        localvar_scope = self._LocalVariableScope.ERROR
        last_fn = None
        last_local_type = ""
        block_scopes = []

        # list of blocks each containing a list of vars scoped to that block. Always local to a function.
        block_scopes = []

        while not self._reader.eof():
            line = self._reader.read_line()

            if line == "":
                continue
            if line.startswith("Data"):
                continue
            if self.SECTION_UNDERLINE.match(line):
                continue

            if module_start_match := self.MODULE_START_REGEX.match(line):

                if current_module is not None:
                    current_module._remove_single_void_arguments()
                    yield current_module

                self._module_count += 1
                current_module = DumpedModule({
                    "name": module_start_match.group(1),
                    "functions": [],
                    "global_vars": [],
                    "types": {},
                    "number": self._module_count,
                })
                last_fn = None
                last_local_type = ''

            elif line == self.LOCALS_SECTION_HEADER:
                self._state = self._ReadState.LOCALS
            elif line == self.TYPES_SECTION_HEADER:
                self._state = self._ReadState.TYPES
            elif self._state == self._ReadState.LOCALS:
                if match := self.LOCAL_HEADER_REGEX.match(line):
                    local_type = match.group(2)

                    if local_type == "MODULE_386":
                        glob = self._process_global_var()
                        current_module.raw_data["global_vars"].append(glob)
                        last_fn = None

                    elif local_type in ("NEAR_RTN_386", "FAR_RTN_386"):
                        fn = self._process_function()
                        fn["block_scopes"] = block_scopes.copy()
                        last_fn = fn
                        current_module.raw_data["functions"].append(fn)
                        localvar_scope = self._LocalVariableScope.FUNCTION
                        block_scopes = []

                    elif local_type == "LOCAL":
                        match localvar_scope:
                            case self._LocalVariableScope.FUNCTION:
                                local_var = self._process_function_var()
                                last_fn["local_vars"].append(local_var)
                            case self._LocalVariableScope.BLOCK:
                                block_var = self._process_function_var()
                                block_scopes[-1].append(block_var)
                            case self._LocalVariableScope.ERROR:
                                raise RuntimeError("Invalid local variable scope")

                    elif local_type == "SET_BASE_386":
                        last_local_type = local_type
                        last_fn = None
                        self._reader.read_line()

                    elif local_type == "BLOCK_386":
                        self._reader.read_line()
                        localvar_scope = self._LocalVariableScope.BLOCK
                        block_scopes.append([])

                    else:
                        logger.error("Line %d: Unknown local type %s", self._reader.linenb, local_type)
                        last_fn = None
                    last_local_type = local_type
                else:
                    logger.error("Line %d: Unknown local type %s", self._reader.linenb, local_type)
            elif self._state == self._ReadState.TYPES:
                if match := self.TYPE_HEADER_REGEX.match(line):
                    t = match.group(2)
                    idx = int(match.group(3))
                    typedef = self._process_type(t)
                    if typedef is not None:
                        typedef["type"] = t
                        typedef["id"] = idx
                        current_module.raw_data["types"][idx] = typedef
                else:
                    pass

        yield current_module

    def _process_global_var(self):
        line = self._reader.read_line()
        match = self.GLOBAL_VAR_REGEX.match(line)
        glob = {
            "name": match.group(1),
            "addr": {k: int(v, 16) for k, v in zip(("section", "offset"), match.group(2).split(":"))},
            "type": int(match.group(3)),
        }
        return glob

    def _process_function(self):
        fn = {
            "args": [],
            "local_vars": [],
        }

        # start off = 00000000, code size = 000000C9, parent off = 0000
        line = self._reader.read_line()
        match = self.FUNCTION_OFFSET_REGEX.match(line)
        fn.update({
            "offset": int(match.group(1), 16),
            "size": int(match.group(2), 16),
            "parent_offset": int(match.group(3), 16),
        })

        # prologue size = 23,  epilogue size = 9
        self._reader.read_line()

        # return address offset (from bp) = 00000004
        self._reader.read_line()

        # return type:  1322
        line = self._reader.read_line()
        parts = line.split(":")
        fn["type"] = int(parts[1].strip())

        # return value: <none>
        line = self._reader.read_line()
        parts = line.split(":")
        val = parts[1].strip()
        if val in ("MULTI_REG(1)", "REG"):
            fn["return_value"] = parts[2].strip()
        elif val == "IND_REG_RALLOC_NEAR( EAX )":
            fn["return_value"] = "EAX"
        elif val == "<none>":
            fn["return_value"] = None
        else:
            fn["return_value"] = None
            logger.error("Line %d: Unhandled return value %s",self._reader.linenb, parts)

        # Parm 0: MULTI_REG(1): EAX
        line = self._reader.read_line()
        while line.startswith("Parm"):
            parts = line.split(":")
            fn["args"].append(parts[2].strip())
            line = self._reader.read_line()

        # Name = "AllocateActorMatrix"
        parts = line.split("=")
        fn["name"] = parts[1].replace('\"', "").strip()

        return fn

    def _process_function_var(self):
        local_var = {}

        #address: BP_OFFSET_BYTE( E4 )
        line = self._reader.read_line()
        match = self.LOCAL_VAR_ADDR_REGEX.match(line)
        local_var.update({
            "addr_type": match.group(1),
            "addr": match.group(2),
        })

        #name = "pTrack_spec",  type = 1320
        line = self._reader.read_line()
        match = self.LOCAL_VAR_REGEX.match(line)
        local_var.update({
            "name": match.group(1).strip(),
            "type": int(match.group(2).strip()),
        })

        return local_var

    def _process_type(self, type_name):
        line = self._reader.read_line()
        if type_name in ("SCOPE", "SCALAR"):
            name = line.split('"')[1].strip()
            return { "value": name }
        elif type_name in ("NEAR386 PTR", "FAR386 PTR"):
            base_type = int(line.split('=')[1].strip())
            return { "base_type": base_type }
        elif type_name in ("BYTE_INDEX ARRAY", "WORD_INDEX ARRAY"):
            # high bound = 00   base type idx = 11
            match = self.ARRAY_TYPE_REGEX.match(line)
            return { "upper_bound": int(match.group(1), 16), "base_type": int(match.group(2)) }
        elif type_name == "NAME":
            #"va_list"  type idx = 12  scope idx = 0
            match = self.NAME_TYPE_REGEX.match(line)
            return { "value": match.group(1), "type_idx": int(match.group(2)), "scope_idx": int(match.group(3)) }

        elif type_name in ("NEAR386 PROC", "FAR386 PROC"):
            # return type = 30
            return_type = int(line.split('=')[1].strip())
            args = []

            # param 1:  type idx = 52
            while (line := self._reader.peek_line()).startswith("param"):
                args.append(int(line.split("=")[1].strip()))
                self._reader.read_line()
            return { "return_type": return_type, "args": args }

        elif type_name == "FIELD_LIST":
            # number of fields = 000D   size = 00000034
            match = self.FIELD_LIST_REGEX.match(line)
            nbr_fields = int(match.group(1), 16)
            struct_size = int(match.group(2), 16)
            fields = []
            for i in range(nbr_fields):
                line = self._reader.read_line()
                match = self.FIELD_HEADER_REGEX.match(line)
                type_name = match.group(2)
                line = self._reader.read_line()
                match = self.FIELD_ITEM_REGEX.match(line)
                field = { "type": type_name, "name": match.group(1), "offset": int(match.group(2), 16), "type_idx": int(match.group(3)) }
                if type_name in ("BIT_BYTE", "BIT_WORD"):
                    # start bit = 1B  bit size = 01
                    line = self._reader.read_line()
                    match = self.FIELD_ITEM_BIT_REGEX.match(line)
                    field["start_bit"] = int(match.group(1), 16)
                    field["bit_size"] = int(match.group(2), 16)
                fields.append(field)
            return { "size": struct_size, "fields": fields }
        elif type_name == "ENUM_LIST":
            match = self.ENUM_LIST_REGEX.match(line)
            nbr_fields = int(match.group(1), 16)
            type_idx = int(match.group(2), 16)
            elements = {}
            for i in range(nbr_fields):
                line = self._reader.read_line()
                line = self._reader.read_line()
                match = self.ENUM_ITEM_REGEX.match(line)
                elements[match.group(1)] = int(match.group(2), 16)
            return { "type_idx": type_idx, "elements": elements }
        else:
            logger.error("%d *** UNEXPECTED: %s", self._reader.linenb, type_name)
            return None


def main():
    import argparse
    import os
    import pprint
    import sys

    logging.basicConfig(format='%(levelname)s: %(message)s')

    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.add_argument("dumpfile", help="Path to output of wdump")
    parser.add_argument("--output", "-o", default=None, help="Output")
    parser.add_argument("--format", "-f", choices=("raw", "pprint", "json", "yml"), default="raw")
    parser.add_argument("-v", dest="verbose", action="store_true", help="Verbose")
    args = parser.parse_args()

    if args.verbose:
        logging.root.setLevel(logging.INFO)

    logger.info("input=%s", args.dumpfile)
    logger.info("output=%s", args.output)

    modules = list(m.raw_data for m in DumpedModuleReader.open(args.dumpfile).read_modules())

    with open(args.output, "w") if args.output else sys.stdout as outstream:
        match args.format:
            case "raw":
                print(modules, file=outstream)
            case "pprint":
                import pprint
                pprint.pprint(modules, stream=outstream)
            case "json":
                import json
                outstream.write(json.dumps(modules, indent=1))
            case "yml":
                import yaml
                yaml.dump(modules, stream=outstream)


if __name__ == "__main__":
    raise SystemExit(main())
