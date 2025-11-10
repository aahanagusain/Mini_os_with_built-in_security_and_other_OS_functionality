#!/usr/bin/env python3
"""
Simple helper to generate a sha256 hex password entry for /etc/passwd.
Usage:
  ./tools/mkpasswd.py username
It will prompt for a password and print a passwd-style line:
  username:hexdigest:uid:gid:comment:home:shell
Defaults: uid=1000 gid=1000 comment='user' home='/home/username' shell='/bin/sh'
"""
import sys
import hashlib
import getpass

def main():
    if len(sys.argv) >= 2:
        user = sys.argv[1]
    else:
        user = input("username: ")
    pwd = getpass.getpass()
    h = hashlib.sha256(pwd.encode('utf-8')).hexdigest()
    uid = '1000'
    gid = '1000'
    comment = 'user'
    home = f"/home/{user}"
    shell = '/bin/sh'
    print(f"{user}:{h}:{uid}:{gid}:{comment}:{home}:{shell}")

if __name__ == '__main__':
    main()
