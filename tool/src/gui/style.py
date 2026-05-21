"""style.py — Shared style constants for BMS Tool GUI."""

# Mode badge: (background hex, foreground hex)
_MODE_PALETTE = {
    'BMS_APP':      ('#2a8a2a', '#ffffff'),
    'BOOTLOADER':   ('#b87800', '#ffffff'),
    'UNSUPPORTED':  ('#c0392b', '#ffffff'),
    'DISCONNECTED': ('#666666', '#ffffff'),
}

MONOSPACE = "Menlo, Consolas, 'Courier New', monospace"

# Semantic status colours — chosen to be readable on both light and dark system backgrounds.
COLOR_OK       = '#27ae60'   # medium green
COLOR_WARN     = '#d4a017'   # medium amber
COLOR_ERROR    = '#c0392b'   # medium red
COLOR_NEUTRAL  = '#888888'   # mid gray
COLOR_MODIFIED = '#2980b9'   # medium blue


def mode_badge_style(mode_name: str) -> str:
    bg, fg = _MODE_PALETTE.get(mode_name, ('#666666', '#ffffff'))
    return (
        f"background-color:{bg}; color:{fg}; "
        "font-weight:bold; padding:2px 10px; border-radius:3px;"
    )


def status_label_style(kind: str) -> str:
    """kind: 'ok' | 'warn' | 'error' | 'neutral' | 'modified'"""
    color = {
        'ok':      COLOR_OK,
        'warn':    COLOR_WARN,
        'error':   COLOR_ERROR,
        'neutral': COLOR_NEUTRAL,
        'modified': COLOR_MODIFIED,
    }.get(kind, COLOR_NEUTRAL)
    return f"color:{color}; font-weight:bold;"


# Structural stylesheet — no background/foreground colour overrides so the system
# palette (including macOS dark mode) applies correctly to all widgets.
APP_STYLESHEET = """
QGroupBox {
    font-weight: bold;
    margin-top: 8px;
    padding-top: 4px;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 8px;
}
"""
