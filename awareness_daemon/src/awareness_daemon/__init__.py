def main():
    # Importing database/model helpers must not initialize Flask, read server
    # configuration, or attempt to discover a controller's credentials.
    from .main import main as run
    return run()

__all__ = ["main"]
