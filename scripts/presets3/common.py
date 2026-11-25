"""Shared constructor for the preset spec files."""


def preset(name, category, desc, note, sustained, tags, body, style=None):
    assert '"' not in desc and '"' not in name and '"' not in tags
    return dict(name=name, category=category, desc=desc, note=note, sustained=sustained, tags=tags, body=body, style=style)
