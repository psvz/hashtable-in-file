# -*- coding: utf-8 -*-
"""
Created on Wed Nov  2 11:51:43 2022

@author: vizo
"""

from os import urandom
from nacl.signing import SigningKey
from nacl.bindings import sodium_increment as incr

sample = 1000000
tolerance = 0.5 # %
endianess = 'little'

sko = SigningKey.generate()

hex = '' # 32 bytes in hex :
for i in range(32): hex += 'ff'

msg = bytes.fromhex(hex)

print(f"{int.from_bytes(msg, endianess):x}")
print(f"Endianess - {endianess} - check, after two increments:")

msg = incr(msg); msg = incr(msg)
print(f"{int.from_bytes(msg, endianess):x} <- must be 1\n")

out = [0 for i in range(512)]

for i in range(sample):
    
    signed = sko.sign(msg)
    #print(signed.signature.hex())
    s = int.from_bytes(signed.signature, endianess) # 64 bytes
    
    for j in range(512):        
        if s & 1 << j: out[j] += 1
        
    sko = SigningKey.generate() #msg = urandom(32) #msg = incr(msg)
    
for i, j in enumerate(out):
    
    s = j * 100 / sample
    if abs(s - 50) > tolerance:
        
        print(f'bit {i}: {s}%')
        
