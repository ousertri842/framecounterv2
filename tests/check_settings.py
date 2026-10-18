import json
import pathlib
import re

root = pathlib.Path(__file__).resolve().parents[1]
manifest = json.loads((root / "mod.json").read_text(encoding="utf-8"))
source = (root / "src/Data.cpp").read_text(encoding="utf-8")
loaded = set(re.findall(r'getSettingValue<[^>]+>\("([^"]+)"\)', source))
settings = manifest["settings"]
expected = {name for name, value in settings.items() if value["type"] != "title"}
# Sounds.cpp loads file settings separately.
expected = {name for name in expected if not name.startswith("sound-")}
assert loaded == expected, (loaded - expected, expected - loaded)
assert settings["exact-scan"]["default"] is True
assert settings["check-to-end"]["default"] is True
assert settings["count-open-windows"]["default"] is False
assert manifest["version"] == "v1.1.0"
print("Settings and accuracy defaults verified")
