from typing import List, Tuple

def findLib(ip_str: str, load_bases: List[Tuple[str, str]]) -> Tuple[str, str]:
    '''Returns a tuple containing the name of the library the given ip belongs to and its load base address'''
    ip = int(ip_str, 16)
    name, addr = max(filter(lambda x: x[1] <= ip, map(lambda x: (x[0], int(x[1], 16)), load_bases)), key = lambda x: x[1])
    return (name, hex(addr))