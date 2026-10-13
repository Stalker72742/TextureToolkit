# Texture Toolkit

> [!WARNING]
> **Work in progress.** The plugin is under active development and has not been fully tested yet.
> Expect bugs and breaking changes. Back up or commit your project before running destructive
> operations (Resize Source, Generate over existing assets, Replace All References).

Editor plugin for Unreal Engine 5 that bundles everyday texture optimization work into one tab:
downscaling textures, packing masks into ORM-like textures, transferring and splitting channels,
and building UI atlases interactively with Paper2D sprites that can replace textures in your widgets.

Open it from **Tools → Texture Toolkit**.

## Features

### Texture browser
A small content browser docked on the left side of the tool.

- Folder tree, search, filters, thumbnails and multi-select (`Ctrl+A` selects everything in the folder).
- Switch between **Textures** and **Widgets** (Widget Blueprints).
- *Include subfolders* toggle.
- Right click opens the toolkit context menu: add to Compress, assign to channels, add to the atlas, use as a base atlas, atlasify widgets.
- Double click adds to the active mode. Assets can also be dragged into any panel, including from the regular Content Browser.

### Compress
Shrink textures and see the memory you save before applying anything.

| Method | What it does |
| --- | --- |
| Max In-Game Size | Sets *Maximum Texture Size*. Source pixels are kept. |
| LOD Bias | Drops top mips through *LOD Bias*. Source pixels are kept. |
| Resize Source | Resamples the source pixels. Also shrinks the asset on disk. Asks for confirmation. |

- Target as a maximum dimension (32 to 8192) or as a scale factor (1/2, 1/4, 1/8).
- Optional overrides: compression settings, sRGB, mip generation, texture group.
- The table shows source size, in-game size, pixel format and estimated memory before and after, plus the total saving.
- Changes go through the transaction system, so Undo works until the textures are saved.

### Channel Pack
- Presets: **ORM**, **RMA**, **MRA**, **Mask Map** (Metallic / AO / Detail / Smoothness), **Metallic + Smoothness**, **Custom**.
- Every output channel takes a texture, the channel to read (R, G, B, A or luminance), an invert flag, and a constant used when no texture is assigned.
- Drop several textures and they are assigned by name: `_AO`, `_Roughness`, `_Metallic`, `_Gloss` / `_Smoothness` (gloss is inverted into roughness automatically), `_Detail`.
- Live 256 px preview of RGB or of a single channel.
- **Channel transfer**: *Use as RGB Source* copies RGB from one texture, then assign another one to Alpha (for example Base Color + Opacity).
- **Split Channels** writes selected channels into separate grayscale textures (`<Name>_R`, `_G`, `_B`, `_A`).
- Values are copied as stored, without gamma conversion, which is what mask maps expect.
- An existing texture with the output name is updated in place, so references stay valid.

### Atlas
Interactive atlas builder in the spirit of classic sprite packers.

- **Canvas**: drag sprites, zoom, pan, context menu, red outline for overlaps or sprites outside the atlas.
- **Auto Arrange** picks the smallest power of two size that fits (or uses a manual size). Locked sprites keep their position.
- Per sprite scale, transparent border trimming, padding with edge extrusion against bleeding.
- **Base atlas**: pick an existing atlas and new sprites go into its free space. Existing sprites and non-transparent pixels are detected and kept. The atlas is updated in place, and existing sprites are refreshed when it grows.
- **Generate** writes the atlas texture and one `PaperSprite` per texture (`SPR_<Name>`). Existing assets with the same names are updated in place.
- **Atlasify Widgets**: add Widget Blueprints and their brush textures are pulled into the atlas. After generation every `FSlateBrush` in those widgets that used one of the textures is switched to the generated sprite, and the blueprints are recompiled.
- **Replace All References** does the same for every asset that references the source textures.

Canvas controls:

| Input | Action |
| --- | --- |
| Left drag | Move sprite (hold `Shift` to snap to 16 px) |
| Arrow keys | Nudge 1 px (16 px with `Shift`) |
| Mouse wheel | Zoom around the cursor |
| Right / middle drag | Pan |
| Right click | Context menu (lock, scale, remove, auto arrange) |
| `F` | Fit the atlas into the view |
| `Delete` | Remove the selected sprite |

## Requirements

- Unreal Engine 5, developed against **UE 5.8** (source build). Other 5.x versions may need small API fixes.
- **Paper2D** (enabled automatically as a plugin dependency).
- Editor only. Nothing is added to packaged builds.

## Installation

Clone into the `Plugins` folder of your project:

```bash
git clone https://github.com/Stalker72742/TextureToolkit.git Plugins/TextureToolkit
```

Or add it as a submodule:

```bash
git submodule add https://github.com/Stalker72742/TextureToolkit.git Plugins/TextureToolkit
```

Then regenerate project files, build the editor target and make sure **Texture Toolkit** is enabled in **Edit → Plugins**.

## Notes and limitations

- UDIM and layered textures are skipped. Only single block, single layer `Texture2D` sources are processed.
- Reference replacement only touches `FSlateBrush` properties. Plain `Texture2D` properties (materials and similar) are left unchanged because a sprite cannot be assigned to them.
- A trimmed sprite placed into a widget brush stretches to the brush size, so the visible area changes. Leave trimming off for widgets that rely on transparent margins.
- Memory figures are estimates based on desktop pixel formats.
- Tool settings are not saved between editor sessions yet.

## License

[MIT](LICENSE)
