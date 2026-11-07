"""HTTPS relay between the PicoCalc FX calculator and the Claude / ChatGPT APIs.

Provider API keys live only in the relay's environment; the calculator
authenticates with a per-device token whose SHA-256 hash is in the config.
"""
__version__ = "1.6.0"
