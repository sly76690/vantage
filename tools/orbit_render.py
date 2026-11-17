# Orbit - renders the artwork ("skin") of the plugin's front panel.
#
# 1. Open Orbit once (standalone or in a DAW): it writes layout.json, the position of every control,
#    in  ~/Library/Application Support/SF Instruments/Vantage/Skin
# 2. Run this script from the project folder:
#       /Applications/Blender.app/Contents/MacOS/Blender -b -P tools/orbit_render.py
#    It builds the whole panel from layout.json and writes, in the same Skin folder:
#       background.jpg   the panel with everything that does not move: paint, print, button caps, unlit LEDs
#       knob.png         one knob body with its shadow (the plugin scales it to each knob)
#       index_*.png      the index line of a knob, in each section colour (the plugin turns it)
#       fader.png        a fader cap with its shadow
#       led.png          the glow of a lit LED
# 3. Reopen Orbit: when all the pictures are there, it shows them instead of the drawn interface.
#    Delete background.jpg to go back to the drawn interface.
#
# Add  -- quick  at the end of the command for a fast, rough render to check positions.
# Everything is built by this script, in Orbit's own design; nothing is imported.

import json
import math
import os
import sys

import bpy
import numpy as np
from mathutils import Vector

QUICK = "quick" in sys.argv

# ---- look settings ----
TILT = 0.0            # degrees: 0 = seen from straight above (keys, panel and controls share one orientation).
                      # If you change it, change tiltCos and topLift in Source/Editor.h (OrbitSkin) to match.
SOFT_WATTS = 0.9      # large soft light from above: overall brightness
KEY_STRENGTH = 2.0    # main light from the upper left, fairly high: highlights and short, soft shadows
RIM_STRENGTH = 0.45   # cool light from the lower right
STRIP_WATTS = 1.0     # long narrow lamp: the bright streak reflected along glossy edges
AMBIENT = 0.02
PANEL_GRAIN = 0.28
FILM_GRAIN = 0.005    # photographic grain on the background (0 = none)
VIGNETTE = 0.10
SAMPLES = 48 if QUICK else 192
PIXELS = 1 if QUICK else 2          # rendered pixels per design pixel (2 = sharp on Retina screens)

SKIN = os.path.expanduser("~/Library/Application Support/SF Instruments/Vantage/Skin")
LAYOUT = os.path.join(SKIN, "layout.json")
if not os.path.exists(LAYOUT):
    raise SystemExit("layout.json not found in " + SKIN + ": open Orbit once so that it writes it, then run this script again.")
with open(LAYOUT, encoding="utf-8") as handle:
    layout = json.load(handle)

W, H = layout["width"], layout["height"]
S = 0.00025                      # metres per design pixel: the panel is 37 cm wide


def X(x):
    return (x - W / 2) * S


COS, SIN = math.cos(math.radians(TILT)), math.sin(math.radians(TILT))
VIEW = Vector((0.0, SIN, -COS))   # direction the camera looks: down, and slightly towards the back of the panel


def Y(y):
    """Design row to position on the panel. Seen at an angle, depth on the panel looks shorter by COS, so the
    panel is laid out deeper by 1/COS: on the picture every control lands exactly on its design position."""
    return (H / 2 - y) * S / COS


AMBER = (0.82, 0.36, 0.035, 1.0)
TEAL = (0.07, 0.42, 0.55, 1.0)
NEUTRAL = (0.66, 0.61, 0.50, 1.0)   # warm cream caps
BRIGHT = (0.78, 0.73, 0.61, 1.0)
INK = (0.90, 0.89, 0.84, 1.0)
DIM = (0.48, 0.47, 0.42, 1.0)

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene


def set_input(shader, names, value):
    for name in names:
        if name in shader.inputs:
            shader.inputs[name].default_value = value
            return


def factor(node):
    return node.outputs.get("Fac") or node.outputs.get("Factor") or node.outputs[-1]


def material(name, colour, roughness, metallic=0.0, grain=0.0, grain_scale=3200.0, coarse=0.0, sheen=0.0,
             coat=0.0, brushed=False):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nodes, links = m.node_tree.nodes, m.node_tree.links
    shader = nodes.get("Principled BSDF")
    shader.inputs["Base Color"].default_value = colour
    shader.inputs["Roughness"].default_value = roughness
    shader.inputs["Metallic"].default_value = metallic
    coords = nodes.new("ShaderNodeTexCoord")
    if coat > 0.0:
        set_input(shader, ["Coat Weight", "Clearcoat"], coat)
        set_input(shader, ["Coat Roughness", "Clearcoat Roughness"], 0.08)
    if sheen > 0.0:
        cloud = nodes.new("ShaderNodeTexNoise")
        cloud.inputs["Scale"].default_value = 55.0
        cloud.inputs["Detail"].default_value = 8.0
        spread = nodes.new("ShaderNodeMapRange")
        spread.inputs["To Min"].default_value = max(0.02, roughness - sheen)
        spread.inputs["To Max"].default_value = min(1.0, roughness + sheen)
        links.new(coords.outputs["Object"], cloud.inputs["Vector"])
        links.new(factor(cloud), spread.inputs["Value"])
        links.new(spread.outputs[0], shader.inputs["Roughness"])
    normal = None
    for scale, strength in ((grain_scale, grain), (70.0, coarse)):
        if strength <= 0.0:
            continue
        noise = nodes.new("ShaderNodeTexNoise")
        noise.inputs["Scale"].default_value = scale
        noise.inputs["Detail"].default_value = 4.0
        height = nodes.new("ShaderNodeBump")
        height.inputs["Strength"].default_value = strength
        links.new(coords.outputs["Object"], noise.inputs["Vector"])
        links.new(factor(noise), height.inputs["Height"])
        if normal is not None:
            links.new(normal, height.inputs["Normal"])
        normal = height.outputs["Normal"]
    if brushed:
        rings = nodes.new("ShaderNodeTexWave")
        rings.wave_type = "RINGS"
        try:
            rings.rings_direction = "Z"
        except Exception:
            pass
        rings.inputs["Scale"].default_value = 9000.0
        rings.inputs["Distortion"].default_value = 1.2
        rings.inputs["Detail"].default_value = 3.0
        height = nodes.new("ShaderNodeBump")
        height.inputs["Strength"].default_value = 0.25
        links.new(coords.outputs["Object"], rings.inputs["Vector"])
        links.new(factor(rings), height.inputs["Height"])
        normal = height.outputs["Normal"]
        set_input(shader, ["Anisotropic"], 0.75)
        try:
            tangent = nodes.new("ShaderNodeTangent")
            tangent.direction_type = "RADIAL"
            tangent.axis = "Z"
            links.new(tangent.outputs[0], shader.inputs["Tangent"])
        except Exception:
            pass
    if normal is not None:
        links.new(normal, shader.inputs["Normal"])
    return m


PANEL = material("panel", (0.0055, 0.006, 0.0065, 1.0), 0.66, 0.0, grain=PANEL_GRAIN, grain_scale=2600.0, coarse=0.04, sheen=0.08)
CHEEK = material("end cheek", (0.035, 0.036, 0.038, 1.0), 0.34, 0.9, grain=0.10, grain_scale=2500.0, sheen=0.10)
PLATE = material("inset plate", (0.012, 0.013, 0.014, 1.0), 0.5, 0.2, grain=0.25, sheen=0.08)
GLASS = material("display window", (0.002, 0.002, 0.0025, 1.0), 0.42)   # dull black: a glossy one mirrored the big light and came out grey
SKIRT = material("knob skirt", (0.008, 0.008, 0.009, 1.0), 0.16, sheen=0.05, coat=0.9)
CAP = material("knob cap", (0.020, 0.021, 0.022, 1.0), 0.14, sheen=0.05, coat=1.0)
METAL = material("turned disc", (0.74, 0.75, 0.76, 1.0), 0.26, 1.0, brushed=True)
STEEL = material("screw", (0.42, 0.43, 0.44, 1.0), 0.38, 1.0, grain=0.05, grain_scale=4000.0, sheen=0.08)
SLOT = material("slot", (0.003, 0.003, 0.003, 1.0), 0.75)
DARK = material("dark print", (0.015, 0.015, 0.015, 1.0), 0.55)
IVORY = material("white key", (0.80, 0.79, 0.74, 1.0), 0.22, sheen=0.06, coat=0.7)
EBONY = material("black key", (0.006, 0.006, 0.007, 1.0), 0.12, sheen=0.04, coat=1.0)
LED_OFF = material("led off", (0.12, 0.012, 0.01, 1.0), 0.15, coat=0.8)
PRINT = {"ink": material("ink print", INK, 0.55, grain=0.08), "dim": material("dim print", DIM, 0.55, grain=0.08)}
PAINT = {name: material(name + " paint", colour, 0.48, grain=0.06, sheen=0.06)
         for name, colour in (("amber", AMBER), ("teal", TEAL), ("neutral", NEUTRAL), ("bright", BRIGHT))}
KEYCAP = {name: material(name + " cap", colour, 0.36, grain=0.06, grain_scale=3500.0, sheen=0.08)
          for name, colour in (("amber", AMBER), ("teal", TEAL), ("neutral", NEUTRAL), ("bright", BRIGHT))}

DARK_CAP = material("dark cap", (0.040, 0.041, 0.043, 1.0), 0.36, grain=0.06, grain_scale=3500.0, sheen=0.08, coat=0.35)
SLIT_OFF = material("light bar off", (0.11, 0.05, 0.012, 1.0), 0.3, coat=0.8)


BAND = {"amber": material("amber band", (0.66, 0.17, 0.010, 1.0), 0.5, grain=0.05, sheen=0.05),      # burnt orange
        "teal": material("teal band", (0.035, 0.24, 0.34, 1.0), 0.5, grain=0.05, sheen=0.05),
        "neutral": material("neutral band", (0.16, 0.155, 0.14, 1.0), 0.5, grain=0.05, sheen=0.05)}


def wood(name, along_x):
    """Dark satin walnut. The grain runs along the length of the part: along x for the rails, along y for the cheeks."""
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nodes, links = m.node_tree.nodes, m.node_tree.links
    shader = nodes.get("Principled BSDF")
    shader.inputs["Roughness"].default_value = 0.34
    set_input(shader, ["Coat Weight", "Clearcoat"], 0.35)
    set_input(shader, ["Coat Roughness", "Clearcoat Roughness"], 0.18)
    coords = nodes.new("ShaderNodeTexCoord")
    stretch = nodes.new("ShaderNodeMapping")
    stretch.inputs["Scale"].default_value = (0.06, 1.0, 1.0) if along_x else (1.0, 0.06, 1.0)
    grain = nodes.new("ShaderNodeTexNoise")
    grain.inputs["Scale"].default_value = 260.0
    grain.inputs["Detail"].default_value = 9.0
    tones = nodes.new("ShaderNodeValToRGB")
    tones.color_ramp.elements[0].position = 0.38
    tones.color_ramp.elements[0].color = (0.050, 0.017, 0.007, 1.0)
    tones.color_ramp.elements[1].position = 0.62
    tones.color_ramp.elements[1].color = (0.340, 0.135, 0.050, 1.0)   # warm, clearly reddish wood
    relief = nodes.new("ShaderNodeBump")
    relief.inputs["Strength"].default_value = 0.10
    links.new(coords.outputs["Object"], stretch.inputs["Vector"])
    links.new(stretch.outputs[0], grain.inputs["Vector"])
    links.new(factor(grain), tones.inputs[0])
    links.new(tones.outputs[0], shader.inputs["Base Color"])
    links.new(factor(grain), relief.inputs["Height"])
    links.new(relief.outputs["Normal"], shader.inputs["Normal"])
    return m


WOOD_RAIL = wood("walnut rail", True)
WOOD_CHEEK = wood("walnut cheek", False)
BADGE = material("badge plate", (0.008, 0.008, 0.009, 1.0), 0.22, coat=0.9)
CHROME = material("badge letters", (0.86, 0.86, 0.84, 1.0), 0.16, 1.0)

FONT = None
for path in ("/System/Library/Fonts/Avenir Next Condensed.ttc", "/System/Library/Fonts/Supplemental/Arial Narrow Bold.ttf"):
    if os.path.exists(path):
        try:
            FONT = bpy.data.fonts.load(path)
            break
        except Exception:
            FONT = None


def finish(obj, mat, bevel=0.0):
    obj.data.materials.append(mat)
    if bevel > 0.0:
        modifier = obj.modifiers.new("bevel", "BEVEL")
        modifier.width = bevel
        modifier.segments = 4
    bpy.context.view_layer.objects.active = obj
    try:
        bpy.ops.object.shade_smooth_by_angle(angle=0.6)
    except Exception:
        bpy.ops.object.shade_smooth()
        if hasattr(obj.data, "use_auto_smooth"):
            obj.data.use_auto_smooth = True
    return obj


def box(location, size, mat, bevel=0.0, turn=0.0):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=location, rotation=(0.0, 0.0, turn))
    obj = bpy.context.object
    obj.scale = size
    bpy.ops.object.transform_apply(scale=True)
    return finish(obj, mat, bevel)


def rect_box(x, y, w, h, height, mat, bevel=0.0, base=0.0):
    """A box covering the design rectangle (x, y, w, h), standing 'height' metres above 'base'."""
    return box((X(x + w / 2), Y(y + h / 2), base + height / 2), (w * S, h * S / COS, height), mat, bevel)


def raised_box(x, y, w, h, top, mat, bevel=0.0):
    """A box standing 'top' metres above the panel whose TOP face lands on the design rectangle (x, y, w, h).
    Its front face then shows just below the rectangle, as it would on the real object."""
    return box((X(x + w / 2), Y(y + h / 2) - top * SIN / COS, top / 2), (w * S, h * S / COS, top), mat, bevel)


def cylinder(location, radius, depth, mat, bevel=0.0):
    bpy.ops.mesh.primitive_cylinder_add(vertices=96, radius=radius, depth=depth, location=location)
    return finish(bpy.context.object, mat, bevel)


def text(body, x, y, size_px, mat, z=0.00004, fit_px=None, align="CENTER", spacing=1.0, shear=0.0, raised=False):
    """Printed text centred on design point (x, y). size_px is the plugin's font height.
    raised: the text is on top of a raised part (z metres up) and must still read at (x, y) on the picture."""
    bpy.ops.object.text_add(location=(X(x), Y(y) - (z * SIN / COS if raised else 0.0), z))
    obj = bpy.context.object
    obj.data.body = body
    if FONT is not None:
        obj.data.font = FONT
    obj.data.size = size_px * S * 0.80
    obj.data.align_x = align
    obj.data.align_y = "CENTER"
    obj.data.space_character = spacing
    obj.data.shear = shear
    obj.data.extrude = 0.00002
    obj.data.materials.append(mat)
    obj.scale.y = 1.0 / COS          # printed deeper on the panel so that it reads at its normal height on the picture
    if fit_px is not None:
        bpy.context.view_layer.update()
        if obj.dimensions.x > fit_px * S > 0:
            obj.scale.x = fit_px * S / obj.dimensions.x
    return obj


MARKS = {"ink": [], "dim": []}   # every printed tick mark, gathered into one mesh per colour


def mark(x, y, w, h, colour="ink", turn=0.0):
    """A flat printed rectangle w x h design pixels centred on design point (x, y), turned by 'turn' radians."""
    cx, cy, hw, hh = X(x), Y(y), w * S / 2, h * S / 2
    corners = []
    for px, py in ((-hw, -hh), (hw, -hh), (hw, hh), (-hw, hh)):
        corners.append((cx + px * math.cos(turn) - py * math.sin(turn), cy + (px * math.sin(turn) + py * math.cos(turn)) / COS, 0.00005))
    MARKS[colour].append(corners)


def build_marks():
    for colour, quads in MARKS.items():
        if not quads:
            continue
        verts = [v for quad in quads for v in quad]
        faces = [(i * 4, i * 4 + 1, i * 4 + 2, i * 4 + 3) for i in range(len(quads))]
        mesh = bpy.data.meshes.new(colour + " marks")
        mesh.from_pydata(verts, [], faces)
        mesh.update()
        obj = bpy.data.objects.new(colour + " marks", mesh)
        scene.collection.objects.link(obj)
        obj.data.materials.append(PRINT[colour])


def light(location, strength, size, colour=(1.0, 1.0, 1.0), kind="AREA"):
    bpy.ops.object.light_add(type=kind, location=location)
    lamp = bpy.context.object
    lamp.data.energy = strength
    lamp.data.color = colour
    lamp.rotation_euler = (Vector((0, 0, 0)) - Vector(location)).to_track_quat("-Z", "Y").to_euler()
    if kind == "AREA":
        if isinstance(size, tuple):   # a long narrow lamp
            lamp.data.shape = "RECTANGLE"
            lamp.data.size, lamp.data.size_y = size
        else:
            lamp.data.size = size
    else:
        lamp.data.angle = size      # a sun: the same light everywhere on the panel
    return lamp


def knob_body(radius):
    return [cylinder((0, 0, radius * 0.19), radius, radius * 0.38, SKIRT, bevel=radius * 0.10),
            cylinder((0, 0, radius * 0.74), radius * 0.74, radius * 0.72, CAP, bevel=radius * 0.14),
            cylinder((0, 0, radius * 1.115), radius * 0.53, radius * 0.04, METAL, bevel=radius * 0.015)]


def knob_index(radius, colour):
    return box((0, radius * 0.60, radius * 1.14), (radius * 0.085, radius * 0.68, radius * 0.035), PAINT[colour], bevel=radius * 0.008)


def aim(target, straight=False):
    """Point the camera at 'target': from the tilted viewpoint, or from straight above when 'straight'."""
    view = Vector((0.0, 0.0, -1.0)) if straight else VIEW
    camera.location = Vector(target) - view
    camera.rotation_euler = view.to_track_quat("-Z", "Y").to_euler()


KNOB_HEIGHT = 1.14                # height of a knob, in units of its radius


def render(path, width, height, ortho, transparent, jpeg=False):
    camera.data.ortho_scale = ortho
    scene.render.resolution_x = int(width)
    scene.render.resolution_y = int(height)
    scene.render.film_transparent = transparent
    scene.render.image_settings.file_format = "JPEG" if jpeg else "PNG"
    if jpeg:
        scene.render.image_settings.quality = 93
    else:
        scene.render.image_settings.color_mode = "RGBA"
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    print("written", path)


# =====================================================================================================
# The panel and everything fixed on it
# =====================================================================================================
bpy.ops.mesh.primitive_plane_add(size=1.0, location=(0, 0, 0))
panel = bpy.context.object
panel.scale = (W * S * 1.1, H * S * 1.1, 1.0)
bpy.ops.object.transform_apply(scale=True)
panel.data.materials.append(PANEL)

# ---- the case: thick walnut cheeks, a rail between the panel and the keys, a front rail, and the maker's badge ----
CHEEK_WIDTH, CHEEK_TOP = 26, 0.008   # low enough for its shadow to stay off the first titles
for x in (0, W - CHEEK_WIDTH):
    rect_box(x, -80, CHEEK_WIDTH, H + 160, CHEEK_TOP, WOOD_CHEEK, bevel=0.0022)
keys_top = layout["keyboard"]["y"]
rail_y, rail_h, RAIL_TOP = keys_top - 17, 14, 0.0055      # slim rail: it must fit between the last row of controls and the keys
raised_box(CHEEK_WIDTH, rail_y, W - 2 * CHEEK_WIDTH, rail_h, RAIL_TOP, WOOD_RAIL, bevel=0.0012)
front_y = keys_top + layout["keyboard"]["h"] + 20
raised_box(CHEEK_WIDTH, front_y, W - 2 * CHEEK_WIDTH, H - front_y + 40, 0.0045, WOOD_RAIL, bevel=0.0012)
block = layout["keyboardBlock"]                                   # the badge sits left of the keys, under the octave buttons
badge_w, badge_h, BADGE_TOP = 116, 24, 0.0012                     # black plate with bright raised letters
badge_x, badge_y = block["x"] + (block["w"] - badge_w) / 2, block["y"] + block["h"] - badge_h - 30
raised_box(badge_x, badge_y, badge_w, badge_h, BADGE_TOP, BADGE, bevel=0.0002)
badge_text = text("ORBIT", badge_x + badge_w / 2, badge_y + badge_h / 2 + 0.5, 18, CHROME, z=BADGE_TOP + 0.00002, spacing=1.6, shear=0.22, raised=True)
badge_text.data.extrude = 0.00018                                 # letters stand proud of the plate
for x in (badge_x + 7, badge_x + badge_w - 7):                    # a rivet at each end
    cylinder((X(x), Y(badge_y + badge_h / 2) - BADGE_TOP * SIN / COS, BADGE_TOP + 0.0001), 1.6 * S, 0.0003, CHROME, bevel=0.0001)
for x, y in ((40, 10), (W - 40, 10)):                             # two screws at the top of the panel
    cylinder((X(x), Y(y), 0.0003), 4.5 * S, 0.0006, STEEL, bevel=0.0002)
    box((X(x), Y(y), 0.00058), (7.5 * S, 1.0 * S, 0.0003), SLOT, turn=(x * 7 + y) % 3)
for x in (CHEEK_WIDTH + 16, W - CHEEK_WIDTH - 16):                # and one at each end of the rail
    cy = Y(rail_y + rail_h / 2) - RAIL_TOP * SIN / COS
    cylinder((X(x), cy, RAIL_TOP + 0.0002), 4.0 * S, 0.0005, STEEL, bevel=0.0002)
    box((X(x), cy, RAIL_TOP + 0.00045), (6.5 * S, 0.9 * S, 0.0003), SLOT, turn=(x % 5) * 0.6)

left, content = layout["left"], layout["contentWidth"]
text("ORBIT", left + 2, layout["headerY"] + 36, 70, PRINT["ink"], align="LEFT", spacing=1.08)
text("106", left + 140, layout["headerY"] + 36, 70, material("amber print", AMBER, 0.55, grain=0.08), align="LEFT", spacing=1.08)
rect_box(left + 228, layout["headerY"] + 16, 1, 40, 0.0001, PRINT["dim"])
text("POLYPHONIC SYNTHESIZER", left + 242, layout["headerY"] + 37, 18, PRINT["dim"], align="LEFT", spacing=1.3, fit_px=280)

display = layout["display"]                                       # the patch display: a dark window under glass
rect_box(display["x"] - 2, display["y"] - 2, display["w"] + 4, display["h"] + 4, 0.0003, CHEEK, bevel=0.0001)
rect_box(display["x"], display["y"], display["w"], display["h"], 0.0004, GLASS)
block = layout["keyboardBlock"]
rect_box(block["x"], block["y"], block["w"], block["h"], 0.0003, PLATE, bevel=0.0001)
keys = layout["keyboard"]
rect_box(keys["x"], keys["y"], keys["w"], keys["h"] + 22, 0.0002, SLOT)              # the dark keybed under the keys


def key(item, top, mat, gap, bevel):
    """One key whose top face, 'top' metres above the panel, lands exactly on its design rectangle.
    Seen at an angle a raised surface appears higher up the picture, so the key is built nearer the viewer
    by the same amount; its front face then shows below the rectangle, which is what gives it depth."""
    w, h = (item["w"] - gap) * S, item["h"] * S / COS
    cx = X(item["x"] + item["w"] / 2)
    cy = (Y(item["y"]) + Y(item["y"] + item["h"])) / 2 - top * SIN / COS
    box((cx, cy, top / 2), (w, h, top), mat, bevel)


for item in layout.get("keys", []):
    if not item["black"]:
        key(item, 0.0110, IVORY, 1.8, 0.0007)
for item in layout.get("keys", []):
    if item["black"]:
        key(item, 0.0175, EBONY, 1.0, 0.0011)

# a dark groove with a light lower edge above each row of sections, like the joint between two panels
for row_y in sorted({section["y"] for section in layout["sections"] if section["x"] == left}):
    rect_box(CHEEK_WIDTH, row_y - 9, W - 2 * CHEEK_WIDTH, 2, 0.00005, SLOT)
    rect_box(CHEEK_WIDTH, row_y - 7, W - 2 * CHEEK_WIDTH, 1, 0.00005, CHEEK)

for section in layout["sections"]:
    # light title printed on the panel, underlined by a thin line in the section's colour
    # a band painted in the section's colour, with the title printed on it
    rect_box(section["x"], section["y"], section["w"], section["h"] + 2, 0.00015, BAND.get(section["colour"], BAND["neutral"]))
    text(section["title"], section["x"] + 8, section["y"] + section["h"] / 2 + 1.5, 25, PRINT["ink"], z=0.00019, align="LEFT",
         fit_px=section["w"] - 14, spacing=1.14)

for item in layout["labels"]:
    if item["text"].strip():
        cx, cy = item["x"] + item["w"] / 2, item["y"] + item["h"] / 2
        on_plate = block["x"] <= cx <= block["x"] + block["w"] and block["y"] <= cy <= block["y"] + block["h"]
        text(item["text"], cx, cy, item["size"] * 1.35, PRINT[item["colour"] if item["colour"] in PRINT else "ink"],
             z=0.00034 if on_plate else 0.00004, fit_px=item["w"])   # captions on the keyboard plate are printed on top of it

SLOT_RIM = material("slot rim", (0.10, 0.10, 0.105, 1.0), 0.28, 1.0, grain=0.05, grain_scale=4000.0, sheen=0.12)

for fader in layout["faders"]:
    cx, top, bottom, marks = fader["cx"], fader["top"], fader["bottom"], max(1, fader["marks"])
    # a lighter metal lip around a narrow black slot
    rect_box(cx - 4.5, top - 9, 9, bottom - top + 18, 0.00022, SLOT_RIM, bevel=0.00012)
    rect_box(cx - 2, top - 7, 4, bottom - top + 14, 0.00028, SLOT)
    # a ladder scale on each side: long bold ticks at 0, 5 and 10, short ones between, joined by a thin rail
    first = not any(o is not fader and o["top"] == top and cx - 46 < o["cx"] < cx for o in layout["faders"])
    for side in (-1, 1):
        mark(cx + side * 17.5, (top + bottom) / 2, 0.8, bottom - top + 1, "dim")
    for i in range(marks + 1):
        yy = top + (bottom - top) * i / marks
        major = marks != 10 or i % 5 == 0
        for side in (-1, 1):
            if major and first and side < 0 and marks == 10:     # the first fader of a group carries the numbers in its ladder
                text(str(10 - i), cx - 12.5, yy, 12.5, PRINT["ink"], fit_px=10)
                continue
            if major:
                mark(cx + side * 12, yy, 11, 1.6)
            else:
                mark(cx + side * 13.5, yy, 8, 0.9, "dim")

for knob in layout["knobs"]:
    if knob["selector"]:
        continue                                                  # selector knobs keep their symbols, drawn by the plugin
    for i in range(11):
        angle = knob["start"] + (knob["end"] - knob["start"]) * i / 10
        long_mark = i % 5 == 0
        inner, outer = knob["d"] / 2 + 2, knob["d"] / 2 + (6 if long_mark else 4)
        r = (inner + outer) / 2
        foot = knob["cy"] + KNOB_HEIGHT * knob["d"] / 2 * SIN / 2      # the knob's picture is centred half-way up the knob
        mark(knob["cx"] + math.sin(angle) * r, foot - math.cos(angle) * r * COS, 1, outer - inner, "dim", turn=-angle)

for button in layout["buttons"]:
    x, y, w, h, led = button["x"], button["y"], button["w"], button["h"], button["led"]
    spot = None
    if led == 1:
        spot, y, h = (x + w / 2, y + 6), y + 12, h - 12
    elif led == 2:
        spot, x, w = (x + 7, y + h / 2), x + 14, w - 14
    rect_box(x + 1, y + 1, w - 2, h - 2, 0.0002, SLOT)                             # the well the cap sits in
    cap_h = h - 5
    dark = button.get("dark", False)          # dark caps for the playing controls, light caps for memories and utilities
    slit = dark and led == 0 and not button["live"]   # a light bar set in the top edge of the cap: the plugin lights it
    if button["live"]:                         # its text changes: a small dark window, the plugin writes in it
        rect_box(x, y, w, h, 0.0003, CHEEK, bevel=0.0001)          # a thin lighter frame, as around the patch display
        rect_box(x + 2, y + 2, w - 4, h - 4, 0.0004, GLASS)
        continue
    rect_box(x + 1.5, y + 1.5, w - 3, cap_h - 1, 0.0016, DARK_CAP if dark else KEYCAP.get(button["colour"], KEYCAP["neutral"]), bevel=0.0005)
    if slit:
        rect_box(x + w / 2 - (w - 2) * 0.22, y + 5, (w - 2) * 0.44, 3, 0.00165, SLIT_OFF)
    if not button["live"] and button["text"].strip():
        text(button["text"], x + w / 2, y + 1 + cap_h / 2 + (2.5 if slit else 0), min(20.0, max(14.0, cap_h * (0.66 if slit else 0.78))),
             PRINT["ink"] if dark else DARK, z=0.00162, fit_px=w - 9)
    if spot is not None:
        bpy.ops.mesh.primitive_uv_sphere_add(radius=1.9 * S, location=(X(spot[0]), Y(spot[1]), 0.00015))
        finish(bpy.context.object, LED_OFF)

build_marks()

# ---- camera and light ----
bpy.ops.object.camera_add(location=(0, 0, 1.0))
camera = bpy.context.object
camera.data.type = "ORTHO"
scene.camera = camera

light((0.0, 0.05, 0.9), SOFT_WATTS, 1.0)
light((-0.34, 0.30, 0.62), KEY_STRENGTH, math.radians(9.0), colour=(1.0, 0.96, 0.90), kind="SUN")
rim = light((0.44, -0.32, 0.20), RIM_STRENGTH, math.radians(8.0), colour=(0.85, 0.92, 1.0), kind="SUN")
for holder, name in ((rim.data, "use_shadow"), (getattr(rim.data, "cycles", None), "cast_shadow")):   # the rim only adds highlights:
    try:                                                                                              # its long shadows looked wrong on the keys
        setattr(holder, name, False)
    except Exception:
        pass
light((-0.10, -0.55, 0.55), STRIP_WATTS, (0.9, 0.05))

world = bpy.data.worlds.new("studio")
world.use_nodes = True
world.node_tree.nodes["Background"].inputs["Strength"].default_value = AMBIENT
scene.world = world

try:
    scene.view_settings.view_transform = "Standard"
    scene.view_settings.look = "None"
except Exception:
    pass
scene.render.engine = "CYCLES"
scene.cycles.samples = SAMPLES
try:
    scene.cycles.use_denoising = True
except Exception:
    pass
os.makedirs(SKIN, exist_ok=True)

background = os.path.join(SKIN, "background.jpg")
aim((0.0, 0.0, 0.0))
render(background, W * PIXELS, H * PIXELS, W * S, transparent=False, jpeg=True)

if FILM_GRAIN > 0.0 or VIGNETTE > 0.0:          # photographic finish on the background
    try:
        image = bpy.data.images.load(background)
        width, height = image.size
        pixels = np.empty(width * height * 4, dtype=np.float32)
        image.pixels.foreach_get(pixels)
        pixels = pixels.reshape(height, width, 4)
        colour = pixels[:, :, :3]
        yy, xx = np.mgrid[0:height, 0:width]
        distance = ((xx / width - 0.5) ** 2 + ((yy / height - 0.5) * height / width) ** 2) / 0.33
        colour *= (1.0 - VIGNETTE * distance)[:, :, None]
        random = np.random.default_rng(106)
        brightness = colour.mean(axis=2, keepdims=True)
        colour += (random.normal(0.0, FILM_GRAIN, (height, width, 1)) + random.normal(0.0, FILM_GRAIN * 0.3, (height, width, 3))) * (0.55 + 0.9 * np.sqrt(np.clip(brightness, 0.0, 1.0)))
        pixels[:, :, :3] = np.clip(colour, 0.0, 1.0)
        image.pixels.foreach_set(pixels.ravel())
        image.filepath_raw = background
        image.file_format = "JPEG"
        image.save()
    except Exception as error:
        print("Photographic finish skipped:", error)

# =====================================================================================================
# The moving parts, each rendered alone on a transparent background with its shadow
# =====================================================================================================
for obj in scene.objects:
    if obj.type not in {"CAMERA", "LIGHT"}:
        obj.hide_render = True

bpy.ops.mesh.primitive_plane_add(size=0.2, location=(0, 0, 0))
catcher = bpy.context.object
try:
    catcher.is_shadow_catcher = True
except Exception:
    pass

FRAME = 96 * PIXELS                   # pixels of each sprite
radius = 24 * S                       # reference knob: 48 design pixels across
body = knob_body(radius)
aim((0.0, 0.0, KNOB_HEIGHT * radius / 2))        # centred half-way up the knob, as the plugin expects
render(os.path.join(SKIN, "knob.png"), FRAME, FRAME, 2 * radius * 1.6, transparent=True)   # 1.6 = OrbitSkin::knobFrame
for part in body:
    part.hide_render = True
catcher.hide_render = True
aim((0.0, 0.0, 0.0), straight=True)             # the index line is rendered flat; the plugin turns and tilts it
for colour in ("amber", "teal", "neutral"):
    line = knob_index(radius, colour)
    render(os.path.join(SKIN, "index_" + colour + ".png"), FRAME, FRAME, 2 * radius * 1.6, transparent=True)
    line.hide_render = True

catcher.hide_render = False
skirt = box((0, 0, 0.0011), (26 * S, 15 * S, 0.0022), DARK_CAP, bevel=0.0005)                # dark base, wider than the top
cap = box((0, 0, 0.0028), (23 * S, 12 * S, 0.0014), KEYCAP["bright"], bevel=0.0005)          # light top, raised on the base
grip = box((0, 0, 0.00352), (21 * S, 1.8 * S, 0.0002), DARK)                                  # bold centre line
for offset in (-3.4, 3.4):
    box((0, offset * S, 0.00352), (19 * S, 0.5 * S, 0.0001), DARK)                            # two fine grooves for the finger
aim((0.0, 0.0, 0.0018))
render(os.path.join(SKIN, "fader.png"), FRAME, FRAME, 48 * S, transparent=True)             # 48 = OrbitSkin::faderFrame

# ---- the glow of a lit LED: painted directly, a bright core in a soft red halo ----
side = 56 * PIXELS
yy, xx = np.mgrid[0:side, 0:side]
r = np.sqrt((xx - side / 2 + 0.5) ** 2 + (yy - side / 2 + 0.5) ** 2) / (side / 2)          # 0 at the centre, 1 at the edge
core = np.clip(1.0 - r / 0.16, 0.0, 1.0) ** 0.6
halo = np.exp(-(r / 0.30) ** 2) * np.clip(1.0 - r, 0.0, 1.0)
glow = np.zeros((side, side, 4), dtype=np.float32)
glow[:, :, 0] = 1.0
glow[:, :, 1] = 0.16 + 0.75 * core
glow[:, :, 2] = 0.06 + 0.45 * core
glow[:, :, 3] = np.clip(core + 0.75 * halo, 0.0, 1.0)
led = bpy.data.images.new("led", side, side, alpha=True)
led.pixels.foreach_set(glow.ravel())
led.filepath_raw = os.path.join(SKIN, "led.png")
led.file_format = "PNG"
led.save()
print("written", led.filepath_raw)
with open(LAYOUT, "rb") as source, open(os.path.join(SKIN, "layout.rendered.json"), "wb") as copy:
    copy.write(source.read())   # the layout these pictures were made for: the plugin only uses them while it still matches
print("Orbit skin complete in", SKIN)
