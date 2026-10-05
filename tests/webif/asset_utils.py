from pathlib import Path
import re


def embedded_asset(root: Path, filename: str, symbol: str) -> str:
    path = root / "webif" / "assets" / filename
    if not path.exists():
        path = root / "webif" / filename
    source = path.read_text(encoding="utf-8")
    match = re.search(
        rf"static const uint8_t {re.escape(symbol)}_DATA\[\] = \{{(.*?)\}};",
        source,
        re.S,
    )
    if not match:
        raise ValueError(f"embedded asset not found: {filename}:{symbol}")
    data = bytes(
        int(token, 0)
        for token in re.findall(r"0x[0-9A-Fa-f]+|\d+", match.group(1))
    )
    return data.rstrip(b"\0").decode("utf-8")
