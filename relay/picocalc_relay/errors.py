class RelayError(Exception):
    """Error returned to the calculator as an HTTP status + short plain-text message."""

    def __init__(self, status: int, message: str):
        super().__init__(message)
        self.status = status
        self.message = message
