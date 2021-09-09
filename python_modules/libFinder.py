from typing import List, Tuple

def findLib(ip_str: str, load_bases: List[Tuple[str, str]]) -> Tuple[str, str]:
    '''Returns a tuple containing the name of the library the given ip belongs to and its load base address'''
    ip = int(ip_str, 16)
    name, addr = max(filter(lambda x: x[1] <= ip, map(lambda x: (x[0], int(x[1], 16)), load_bases)), key = lambda x: x[1])
    return (name, hex(addr))


def findLibByName(name: str, load_bases: List[Tuple[str, str]]) -> str:
    '''Returns the base address of the library whose name is passed as |name|'''
    for lib_name, lib_addr in load_bases:
        if lib_name == name:
            return lib_addr