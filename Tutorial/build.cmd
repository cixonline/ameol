@echo off
del big.one
copy *.txt big.one
txt2scp.exe big.one
del big.one