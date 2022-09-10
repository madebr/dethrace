#!/usr/bin/env python

import argparse
import cmd
import json
import requests
import shlex


def fetch_data_from_hook_server(response):
    if response.status_code != 200:
        raise RuntimeError(f"Got statuscode {response.status_code}")
    try:
        json_data = response.json()
    except json.decoder.JSONDecodeError as e:
        raise RuntimeError(f"'{url}' did not return valid json") from e
    try:
        if json_data["code"] != 0:
            raise RuntimeError(f"Result code was not 0, but was{json_data['code']}")
    except KeyError:
        raise RuntimeError(f"No code key present in json data")
    try:
        return json_data["data"]
    except KeyError:
        raise RuntimeError("Json data did not contain a data member")


def load_all_data(url: str):
    info_data = fetch_data_from_hook_server(requests.get(f"{url}/info"))
    state_stoi = info_data["states"]

    hooks_data = fetch_data_from_hook_server(requests.get(f"{url}/hooks"))

    return state_stoi, hooks_data


class HookClient:
    def __init__(self, url: str):
        self.url = url
        self.update_all()
        
    def update_all(self):
        self.state_stoi, self.hooks = load_all_data(self.url)
        self.state_itos = {v: k for (k, v) in self.state_stoi.items()}

    def set_hook_state(self, hook_i: int, state: int) -> bool:
        try:
            data = fetch_data_from_hook_server(requests.post(f"{self.url}/hook/{hook_i}", json={"state": state}))
            self.hooks[hook_i]["state"] = state
            return True
        except RuntimeError:
            return False


class HookClientCmd(cmd.Cmd):
    def __init__(self, client: HookClient, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.client = client
        self._after_update_all()

    def _on_unknown_argument(self) -> bool:
        self.stdout.write("Unknown argument\n")
        return False

    def _after_update_all(self):
        if len(self.client.state_stoi) != 3:
            self.stdout.write("WARNING: Incorrect number of hook states")
        self.state2shortstr = {
            self.client.state_stoi["enabled"]: "on",
            self.client.state_stoi["disabled"]: "off",
            self.client.state_stoi["unavailable"]: "n/a",
        }

    def do_update(self, arg):
        "Update client state"
        self.client.update_all()
        self._after_update_all()
        return False

    def do_EOF(self, arg):
        "Exit the client"
        return True

    def do_exit(self, arg):
        "Exit the client"
        if arg:
            return self._on_unknown_argument()
        return True

    def do_quit(self, arg):
        "Exit the client"
        if arg:
            return self._on_unknown_argument()
        return True

    def do_show(self, arg):
        "Show all hooks, filter by argument"
        args = shlex.split(arg)
        if len(args) == 0:
            needle = ""
        elif len(args) == 1:
            needle = args[0]
        else:
            self.output("Too many argments")
            return self._on_unknown_argument()
        needle = needle.lower()
        for hook_i, hook_data in enumerate(self.client.hooks):
            if needle in hook_data["name"].lower():
                self.stdout.write(f"{hook_i:>5} | {self.state2shortstr[hook_data['state']]:>3} | {hook_data['name']:<74} | {hook_data['file']}\n")
        return False

    def _str_to_state(self, s: str) -> int:
        try:
            return int(s)
        except ValueError:
            pass
        if s.lower() in ("true", "on", "enable", "enabled"):
            return self.client.state_stoi["enabled"]
        if s.lower() in ("false", "off", "disable", "disabled"):
            return self.client.state_stoi["disabled"]
        raise ValueError

    def do_set(self, arg):
        "Modify state of hooks (arguments: 'hook state')"
        args = shlex.split(arg)
        if len(args) != 2:
            self.stdout.write("Wrong number of arguments\n")
            return False

        needle = args[0]

        try:
            new_state = self._str_to_state(args[1])
        except ValueError as e:
            self.stdout.write(f"Invalid state: {args[1]}\n")
            return False

        count = 0
        for hook_i, hook_data in enumerate(self.client.hooks):
            if needle in hook_data["name"].lower():
                self.stdout.write(f"Modifying state of {hook_data['name']} ...")
                if self.client.set_hook_state(hook_i, new_state):
                    count += 1
                    self.stdout.write(" OK!\n")
                else:
                    self.stdout.write(" FAIL!\n")
        return False

    def _set_state_of_multiple_names(self, names: list[str], new_state: int):
        real_names = []
        for name in names:
            try:
                v = int(name)
                name = self.client.hooks[v]["name"]
            except (ValueError, IndexError):
                pass
            real_names.append(name)

        count = 0
        for hook_i, hook_data in enumerate(self.client.hooks):
            if hook_data["name"] in real_names:
                self.stdout.write(f"Modifying state of {hook_data['name']} ...")
                if self.client.set_hook_state(hook_i, new_state):
                    count += 1
                    self.stdout.write(" OK!\n")
                else:
                    self.stdout.write(" FAIL!\n")
        self.stdout.write(f"Modified {count}/{len(names)} hooks\n")

    def do_enable(self, arg):
        "Enable hooking of multiple hooks (arguments: 'hook [hook [hook ...]]')"
        args = shlex.split(arg)

        new_state = self._str_to_state("1")

        self._set_state_of_multiple_names(args, new_state)
        return False

    def do_disable(self, arg):
        "Disable hooking of multiple hooks (arguments: 'hook [hook [hook ...]]')"
        args = shlex.split(arg)

        new_state = self._str_to_state("0")

        self._set_state_of_multiple_names(args, new_state)
        return False


def main() -> int:
    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.add_argument("url", default="http://localhost:8888", nargs="?", help="url of the REST hook server")
    args = parser.parse_args()

    client = HookClient(url=args.url)

    client_cmd = HookClientCmd(client)
    client_cmd.cmdloop()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
