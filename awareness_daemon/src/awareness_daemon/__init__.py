def main():
    # Importing database/model helpers must not initialize Flask, read server
    # configuration, or attempt to discover a controller's credentials.
    from .main import main as run
    # Importing the submodule sets this package's "main" attribute to the
    # module. Restore the callable entrypoint for subsequent callers.
    globals()["main"] = run
    return run()

__all__ = ["main"]
