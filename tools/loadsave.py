#!/usr/bin/env python

import argparse
import io
import pathlib
import struct
import sys
import yaml


class SaveGamePacker:
    def __init__(self, bigendian: bool):
        self.bigendian = bigendian

        fmt_slot_name = "c" * 16
        fmt_car_names = "c" * 16
        fmt_player_name = "c" * 2 * 14
        fmt_race_info = "I" * 100
        fmt_opponent_info = "I" * 48

        fmt_credits = "I"
        fmt_rank = "I"
        fmt_skill_level = "I"
        fmt_game_completed = "I"
        fmt_number_of_cars = "I"
        fmt_cars_available = "I" * 60
        fmt_current_car_index = "I"
        fmt_current_race_index = "I"
        fmt_redo_race_index = "i"
        fmt_frank_or_annitude = "I"
        fmt_power_up_levels = "I" * 3
        fmt_version = "I"

        fmt_checksum = "I"

        fmt_part1 = "@" \
            + fmt_slot_name \
            + fmt_car_names \
            + fmt_player_name \
            + fmt_race_info \
            + fmt_opponent_info
        fmt_part2 = (">" if self.bigendian else "@") \
            + fmt_credits \
            + fmt_rank \
            + fmt_skill_level \
            + fmt_game_completed \
            + fmt_number_of_cars \
            + fmt_cars_available \
            + fmt_current_car_index \
            + fmt_current_race_index \
            + fmt_redo_race_index \
            + fmt_frank_or_annitude \
            + fmt_power_up_levels \
            + fmt_version
        fmt_part3 = "@" \
            + fmt_checksum

        self.struct1 = struct.Struct(fmt_part1)
        self.struct2 = struct.Struct(fmt_part2)
        self.struct3 = struct.Struct(fmt_part3)

        assert sum(strct.size for strct in (self.struct1, self.struct2, self.struct3)) == 948

    def unpack(self, data: bytes):
        start = 0
        end = self.struct1.size
        part1 = self.struct1.unpack(data[start:end])

        assert len(part1) == 208
        slot_name = b"".join(part1[0:16])
        car_name = b"".join(part1[16:32])
        player_name0 = b"".join(part1[32:46])
        player_name1 = b"".join(part1[46:60])
        race_infos = part1[60:160]
        opponent_info = part1[160:208]

        start = end
        end += self.struct2.size
        part2 = self.struct2.unpack(data[start:end])

        assert len(part2) == 73
        credits = part2[0]
        rank = part2[1]
        skill_level = part2[2]
        game_completed = part2[3]
        number_of_cars = part2[4]
        cars_available = part2[5:65]
        current_car_index = part2[65]
        current_race_index = part2[66]
        redo_race_index = part2[67]
        frank_or_annitude = part2[68]
        power_up_levels = part2[69:72]
        version = part2[72]

        start = end
        end += self.struct3.size
        part3 = self.struct3.unpack(data[start:end])

        assert len(part3) == 1
        checksum = part3[0]

        return {
            "slot_name": slot_name,
            "car_name": car_name,
            "player_name": [player_name0, player_name1],
            "race_infos": race_infos,
            "opponent_info": opponent_info,
            "credits": credits,
            "rank": rank,
            "skill_level": skill_level,
            "game_completed": game_completed,
            "number_of_cars": number_of_cars,
            "cars_available": cars_available,
            "current_car_index": current_car_index,
            "current_race_index": current_race_index,
            "redo_race_index": redo_race_index,
            "frank_or_annitude": frank_or_annitude,
            "power_up_levels": power_up_levels,
            "version": version,
            "checksum": checksum,
        }

    def pack(self, dikt: dict):
        def make_byte_list(bytestr, length):
            if isinstance(bytestr, str):
                bytestr = bytestr.encode()
            assert isinstance(bytestr, bytes)
            if len(bytestr) > length:
                raise ValueError(f"{bytestr} is too long, size is limited to {length} bytes")
            bytestr = bytestr[:length]
            if len(bytestr) < length:
                bytestr += b"\x00" * (length - len(bytestr))
            return [chr(b).encode() for b in bytestr]
        def make_list(l, length):
            if len(l) > length:
                raise ValueError(f"list is too long, size is limited to {length} elements")
            l = l[:length]
            if len(l) < length:
                l += [0] * (length - len(l))
            return l

        slot_name = make_byte_list(dikt["slot_name"], 16)
        car_name = make_byte_list(dikt["car_name"], 16)
        player_name0 = make_byte_list(dikt["player_name"][0], 14)
        player_name1 = make_byte_list(dikt["player_name"][1], 14)
        race_infos = make_list(dikt["race_infos"], 100)
        opponent_info = make_list(dikt["opponent_info"], 48)

        data1 = self.struct1.pack(*slot_name, *car_name, *player_name0, *player_name1, *race_infos, *opponent_info)

        credits = dikt["credits"]
        rank = dikt["rank"]
        skill_level = dikt["skill_level"]
        game_completed = dikt["game_completed"]
        number_of_cars = dikt["number_of_cars"]
        cars_available = dikt["cars_available"]
        current_car_index = dikt["current_car_index"]
        current_race_index = dikt["current_race_index"]
        redo_race_index = dikt["redo_race_index"]
        frank_or_annitude = dikt["frank_or_annitude"]
        power_up_levels = dikt["power_up_levels"]
        version = dikt["version"]

        data2 = self.struct2.pack(credits, rank, skill_level, game_completed, number_of_cars, *cars_available, current_car_index, current_race_index, redo_race_index, frank_or_annitude, *power_up_levels, version)

        checksum = dikt["checksum"]
        data3 = self.struct3.pack(checksum)

        result =  data1 + data2 + data3

        assert len(result) == 948
        return result

def bytestr_to_str(bytestr: bytes) -> str:
    try:
        bytestr = bytestr[:bytestr.index(b"\x00")]
    except ValueError:
        pass
    return bytestr.decode()


def apply_hash(data):
    assert isinstance(data, bytes)
    checksum = 0
    for byte in data:
        checksum2 = (checksum + (byte ^ 0xbd)) & 0xffffffff
        checksum = (checksum ^ (checksum2 << 25) ^ (checksum2 >> 7)) & 0xffffffff
    return checksum


def calculate_checksum(dikt):
    packer = SaveGamePacker(False)
    bindata = packer.pack(dikt)
    return apply_hash(bindata[:-4])


def make_dict_readable(data):
    data["slot_name"] = bytestr_to_str(data["slot_name"])
    data["car_name"] = bytestr_to_str(data["car_name"])
    data["player_name"][0] = bytestr_to_str(data["player_name"][0])
    data["player_name"][1] = bytestr_to_str(data["player_name"][1])


def binary_to_dict(data):
    assert len(data) == 948

    packer = SaveGamePacker(True)
    dikt = packer.unpack(data)
    del dikt["checksum"]
    make_dict_readable(dikt)
    return dikt


def dict_to_binary(dikt):
    dikt["checksum"] = 0
    checksum = calculate_checksum(dikt)
    dikt["checksum"] = checksum

    packer = SaveGamePacker(True)
    data = packer.pack(dikt)
    return data


def template_dikt(splatpack):
    return {
        "car_name": "NEWEAGLE.TXT" if splatpack else "BLKEAGLE.TXT",
        "cars_available": [0] * 60,
        "credits": 5000,
        "current_car_index": 0,
        "current_race_index": 0,
        "frank_or_annitude": 0,
        "game_completed": 0,
        "number_of_cars": 1,
        "opponent_info": [0] * 48,
        "player_name": ["MAX DAMAGE", "DIE ANNA"],
        "power_up_levels": [0, 0, 0],
        "race_infos": [0] * 100,
        "rank": 99,
        "redo_race_index": -1,
        "skill_level": 1,
        "slot_name": "PYLOVE",
        "version": 6,
    }


def main():
    parser = argparse.ArgumentParser(description="Create and manipulate dethrace savegames. By default", allow_abbrev=False)
    parser.add_argument("-t", action="store_true", dest="template", help="Use template (path is ignored)")
    parser.add_argument("-r", action="store_true", dest="reverse", help="")
    parser.add_argument("--splatpack", action="store_true", help="Use splatpack template")
    parser.add_argument("path", type=pathlib.Path, nargs="?", help="Input savegame. Binary file unless -r is given, then must be .yml")
    args = parser.parse_args()

    if not args.template and args.path is None:
        parser.error("PATH is required when not using template")

    def open_stream():
        if args.template:
            template = template_dikt(args.splatpack)
            if args.reverse:
                buffer = io.StringIO()
                yaml.safe_dump(template, buffer)
            else:
                buffer = io.BytesIO()
                data = dict_to_binary(template)
                buffer.write(data)
                assert buffer.tell() == 948
            buffer.seek(0)
            return buffer
        else:
            return open(args.path, "rb")

    with open_stream() as stream:
        if args.reverse:
            dikt = yaml.safe_load(stream)
            data = dict_to_binary(dikt)
            sys.stdout.buffer.write(data)
        else:
            data = stream.read(948)
            dikt = binary_to_dict(data)
            yaml.safe_dump(dikt, sys.stdout)

if __name__ == "__main__":
    raise SystemExit(main())
