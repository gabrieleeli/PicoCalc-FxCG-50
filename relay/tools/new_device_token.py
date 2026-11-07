#!/usr/bin/env python3
"""Creates a random device token. Put the token in the calculator
(SYSTEM > Token relay) and only its SHA-256 digest in relay.json."""
import hashlib
import secrets
import sys

name = sys.argv[1] if len(sys.argv) > 1 else "picocalc"
token = secrets.token_urlsafe(32)
print(f"Token per la calcolatrice ({name}): {token}")
print('Voce per relay.json: {"name": "%s", "token_sha256": "%s"}' % (name, hashlib.sha256(token.encode()).hexdigest()))
