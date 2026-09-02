import os
import urllib.request
import json
import sys

def get_root_dir():
    """Return the absolute path of the repository root directory."""
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def load_env():
    """Load environment variables from a local .env file if it exists."""
    env_path = os.path.join(get_root_dir(), ".env")
    if not os.path.exists(env_path):
        print("Warning: .env file not found. Falling back to system environment variables.")
        return
    with open(env_path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            if "=" in line:
                key, val = line.split("=", 1)
                os.environ[key.strip()] = val.strip()

def main():
    load_env()
    root_dir = get_root_dir()

    token = os.getenv("GITHUB_TOKEN")
    owner = os.getenv("GITHUB_USER")
    repo = os.getenv("GITHUB_REPO")
    tag = sys.argv[1] if len(sys.argv) > 1 else "v1.2.0"

    if not token or not owner or not repo:
        print("Error: GITHUB_TOKEN, GITHUB_USER, and GITHUB_REPO must be set in your .env file or environment.")
        sys.exit(1)

    print(f"Creating/Checking release '{tag}' for {owner}/{repo}...")

    # 1. Create or retrieve the release
    url = f"https://api.github.com/repos/{owner}/{repo}/releases"
    body_text = (
        "## What's Changed in v1.2.0 (Universal Release)\n\n"
        "- **Cross-Platform Universal Support**: Added native **Linux** implementation (Qt6 + PulseAudio/PipeWire + Kernel evdev).\n"
        "- **Global Hotkey (`Ctrl + Alt + Space`)**: Works across all Linux desktops (Wayland/X11) via direct low-level kernel evdev.\n"
        "- **Hardware-accelerated HUD & Tray**: Instant visual feedback and tray status on both Windows & Linux.\n"
        "- **Zero-polling Audio Monitor**: Subscribes directly to audio subsystem events for real-time state sync.\n"
        "- **Universal Packaging**: Windows portable executable, Windows NSIS installer, Linux standalone binary and tarball."
    )
    data = {
        "tag_name": tag,
        "name": f"Microphone Indicator {tag} (Universal)",
        "body": body_text,
        "draft": False,
        "prerelease": False
    }
    
    req = urllib.request.Request(
        url,
        data=json.dumps(data).encode("utf-8"),
        headers={
            "Authorization": f"token {token}",
            "Accept": "application/vnd.github.v3+json",
            "Content-Type": "application/json",
            "User-Agent": "Antigravity-AI-Release-Uploader"
        },
        method="POST"
    )

    release = None
    try:
        with urllib.request.urlopen(req) as res:
            release = json.loads(res.read().decode("utf-8"))
            print(f"Successfully created release '{tag}'!")
    except Exception as e:
        print(f"Release creation skipped (might already exist): {e}")
        # Fetch the existing release by tag
        tag_url = f"https://api.github.com/repos/{owner}/{repo}/releases/tags/{tag}"
        tag_req = urllib.request.Request(
            tag_url,
            headers={
                "Authorization": f"token {token}",
                "Accept": "application/vnd.github.v3+json",
                "User-Agent": "Antigravity-AI-Release-Uploader"
            },
            method="GET"
        )
        try:
            with urllib.request.urlopen(tag_req) as res:
                release = json.loads(res.read().decode("utf-8"))
        except Exception as err:
            print(f"Error fetching existing release: {err}")
            sys.exit(1)

    if not release or "upload_url" not in release:
        print("Error: Could not retrieve release upload URL.")
        sys.exit(1)

    upload_url = release["upload_url"].split("{")[0]

    # 2. Upload assets (Windows + Linux)
    existing_assets = release.get("assets", [])
    assets = [
        os.path.join(root_dir, "releases", "microphone-indicator-windows.exe"),
        os.path.join(root_dir, "releases", "microphone-indicator-setup.exe"),
        os.path.join(root_dir, "releases", "microphone-indicator-linux"),
        os.path.join(root_dir, "releases", "microphone-indicator-linux-x86_64.tar.gz")
    ]

    for full_path in assets:
        if not os.path.exists(full_path):
            print(f"Warning: Asset file not found: {full_path}. Skipping.")
            continue

        filename = os.path.basename(full_path)

        # If asset already exists, delete it first so we can upload the updated binary
        for existing in existing_assets:
            if existing.get("name") == filename:
                del_url = f"https://api.github.com/repos/{owner}/{repo}/releases/assets/{existing['id']}"
                del_req = urllib.request.Request(
                    del_url,
                    headers={
                        "Authorization": f"token {token}",
                        "Accept": "application/vnd.github.v3+json",
                        "User-Agent": "Antigravity-AI-Release-Uploader"
                    },
                    method="DELETE"
                )
                try:
                    with urllib.request.urlopen(del_req) as res:
                        print(f"Removed previous asset '{filename}' from release.")
                except Exception as del_err:
                    print(f"Could not delete old asset '{filename}': {del_err}")

        target_url = f"{upload_url}?name={filename}"
        print(f"Uploading {filename}...")

        with open(full_path, "rb") as f:
            file_bytes = f.read()

        upload_req = urllib.request.Request(
            target_url,
            data=file_bytes,
            headers={
                "Authorization": f"token {token}",
                "Accept": "application/vnd.github.v3+json",
                "Content-Type": "application/octet-stream",
                "User-Agent": "Antigravity-AI-Release-Uploader"
            },
            method="POST"
        )

        try:
            with urllib.request.urlopen(upload_req) as res:
                print(f"Successfully uploaded {filename}!")
        except Exception as e:
            print(f"Error uploading {filename}: {e}")

if __name__ == "__main__":
    main()
