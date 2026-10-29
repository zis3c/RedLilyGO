# LilyGO T-Dongle-S3 presentation model

- `lilygo-t-dongle-s3-3d-exploded.glb`: device with removable cap displayed beside it.
- `lilygo-t-dongle-s3-3d-assembled.glb`: cap fitted over the USB connector.
- `lilygo-t-dongle-s3-3d-source.blend`: editable model and studio cameras/lights.
- `lilygo-t-dongle-s3-render-front.png`, `lilygo-t-dongle-s3-render-back.png`: renders of the exported GLB after reimport into Blender.

The reference photographs are in `docs/images`. The rear component arrangement follows the multi-angle reference. The back-view photograph shows a different PCB revision with different silkscreen and component placement; these two revisions are not an exact match.

Overall capped dimensions: approximately 58.6 x 17.9 x 8.7 mm. Individual component dimensions and traces are visual approximations from photographs, not measured CAD or an electrically accurate PCB. Casing transparency uses glTF alpha blending to expose the internal detail. Appearance depends on the viewer's lighting and transparency rendering.

The model includes hollow housing and cap, actual USB openings, connector contacts, LCD lettering, IC leads, solder joints, switch, antenna, passives, and silkscreen. No external textures are required.

Regenerate on Windows with Python 3.13 and Blender's Python package:

```powershell
python -m pip install bpy==5.2.2
python tools/create_t_dongle_glb.py
```

Uses Windows Arial Bold for text meshes. The generator checks GLB JSON/chunks,
buffer bounds, triangle indices, alpha materials, and capped dimensions. It
reimports the exported model for front/back rendering. PowerPoint import
compatibility is unverified.
