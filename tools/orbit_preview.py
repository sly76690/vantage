# Orbit - photoreal preview of a sample of the front panel (second pass: realism and grain).
#
# Renders design/orbit_preview.png: a piece of panel, three knobs, two faders, two LED buttons and two
# screws, seen straight from above. Compared with the first pass:
#   - raking light from the upper left, so the grain of the panel and the edges of the knobs catch highlights;
#   - materials with life in them: uneven sheen on the paint, lacquered plastic, a brushed metal disc;
#   - the lit LED really throws red light on the panel;
#   - a photographic finish applied after the render: film grain and a slight vignette.
#
# Run from the project folder:
#   /Applications/Blender.app/Contents/MacOS/Blender -b -P tools/orbit_preview.py
#
# Everything is built by this script, in Orbit's own design; nothing is imported.

import math
import os

import bpy
import numpy as np
from mathutils import Vector

# ---- look settings: the values worth playing with ----
SOFT_WATTS = 0.9      # large soft light from above: overall brightness
KEY_WATTS = 1.1       # small raking light from the upper left: highlights, grain of the panel
RIM_WATTS = 0.35      # cool light from the lower right: separates the knobs from the panel
AMBIENT = 0.02
LED_GLOW = 0.02       # red light thrown on the panel by a lit LED
PANEL_GRAIN = 0.35    # relief of the paint
FILM_GRAIN = 0.016    # photographic grain added after the render (0 = none)
VIGNETTE = 0.16       # darkening towards the corners (0 = none)
SAMPLES = 256

OUT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "design", "orbit_preview.png"))

AMBER = (0.82, 0.36, 0.035, 1.0)
TEAL = (0.07, 0.42, 0.55, 1.0)
PAINT = (0.90, 0.89, 0.84, 1.0)

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
             coat=0.0, brushed=False, emission=0.0):
    """grain: fine relief. coarse: slow unevenness. sheen: how much the gloss varies across the surface.
    coat: a lacquer layer. brushed: concentric brush marks and stretched highlights, for a turned metal disc."""
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
    if sheen > 0.0:  # no real surface is evenly glossy
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
        rings.inputs["Scale"].default_value = 5200.0
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
    if emission > 0.0:
        glow = nodes.new("ShaderNodeEmission")
        glow.inputs["Color"].default_value = colour
        glow.inputs["Strength"].default_value = emission
        links.new(glow.outputs["Emission"], nodes.get("Material Output").inputs["Surface"])
    return m


PANEL = material("panel", (0.020, 0.022, 0.025, 1.0), 0.46, 0.25, grain=PANEL_GRAIN, coarse=0.05, sheen=0.10)
SKIRT = material("knob skirt", (0.010, 0.010, 0.011, 1.0), 0.24, grain=0.02, grain_scale=5000.0, sheen=0.07, coat=0.5)
CAP = material("knob cap", (0.035, 0.036, 0.038, 1.0), 0.20, sheen=0.06, coat=0.6)
METAL = material("turned disc", (0.74, 0.75, 0.76, 1.0), 0.26, 1.0, brushed=True)
STEEL = material("screw", (0.42, 0.43, 0.44, 1.0), 0.38, 1.0, grain=0.05, grain_scale=4000.0, sheen=0.08)
WHITE = material("white print", PAINT, 0.55, grain=0.08)
DARK = material("dark print", (0.015, 0.015, 0.015, 1.0), 0.55)
SLOT = material("slot", (0.003, 0.003, 0.003, 1.0), 0.75)
KEYCAP = material("button cap", (0.62, 0.61, 0.57, 1.0), 0.36, grain=0.06, grain_scale=3500.0, sheen=0.08)
LED_OFF = material("led off", (0.12, 0.012, 0.01, 1.0), 0.15, coat=0.8)
LED_ON = material("led on", (1.0, 0.14, 0.05, 1.0), 0.3, emission=9.0)
PAINTS = {"amber": material("amber print", AMBER, 0.48, grain=0.06, sheen=0.06),
          "teal": material("teal print", TEAL, 0.48, grain=0.06, sheen=0.06)}


def finish(obj, mat, bevel=0.0):
    obj.data.materials.append(mat)
    if bevel > 0.0:
        modifier = obj.modifiers.new("bevel", "BEVEL")
        modifier.width = bevel
        modifier.segments = 5
    bpy.context.view_layer.objects.active = obj
    try:
        bpy.ops.object.shade_smooth_by_angle(angle=0.6)   # Blender 4.1 and later
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


def cylinder(location, radius, depth, mat, bevel=0.0):
    bpy.ops.mesh.primitive_cylinder_add(vertices=128, radius=radius, depth=depth, location=location)
    return finish(bpy.context.object, mat, bevel)


def label(text, location, size, mat):
    bpy.ops.object.text_add(location=location)
    obj = bpy.context.object
    obj.data.body = text
    obj.data.size = size
    obj.data.align_x = "CENTER"
    obj.data.align_y = "CENTER"
    obj.data.extrude = 0.00003
    obj.data.materials.append(mat)
    return obj


def light(location, watts, size, colour=(1.0, 1.0, 1.0), kind="AREA"):
    bpy.ops.object.light_add(type=kind, location=location)
    lamp = bpy.context.object
    lamp.data.energy = watts
    lamp.data.color = colour
    if kind == "AREA":
        lamp.data.size = size
        lamp.rotation_euler = (Vector((0, 0, 0)) - Vector(location)).to_track_quat("-Z", "Y").to_euler()
    else:
        lamp.data.shadow_soft_size = size
    return lamp


def knob(x, y, colour, value, title):
    """One Orbit knob at (x, y) on the panel. value 0..1 turns it across its 270 degree travel."""
    angle = math.radians(135.0 - 270.0 * value)
    for i in range(11):                                    # the scale printed on the panel
        a = math.radians(135.0 - 27.0 * i)
        long_mark = i % 5 == 0
        r = 0.0195 + (0.0014 if long_mark else 0.0009)
        box((x - math.sin(a) * r, y + math.cos(a) * r, 0.00004), (0.00045, 0.0028 if long_mark else 0.0018, 0.00006), WHITE, turn=a)
    cylinder((x, y, 0.003), 0.0155, 0.006, SKIRT, bevel=0.0016)
    cylinder((x, y, 0.0115), 0.0115, 0.011, CAP, bevel=0.0022)
    cylinder((x, y, 0.0172), 0.0082, 0.0006, METAL, bevel=0.00025)
    reach = 0.0092                                          # index line: from the cap out over the skirt
    box((x - math.sin(angle) * reach, y + math.cos(angle) * reach, 0.0176), (0.0013, 0.0105, 0.0005), PAINTS[colour], bevel=0.00012, turn=angle)
    label(title, (x, y + 0.029, 0.00004), 0.0042, WHITE)


def fader(x, y, travel, value, title):
    box((x, y, 0.0002), (0.0032, travel + 0.012, 0.0004), SLOT)
    for i in range(11):
        yy = y - travel / 2 + travel * i / 10
        width = 0.0045 if i % 5 == 0 else 0.003
        for side in (-1, 1):
            box((x + side * (0.006 + width / 2), yy, 0.00004), (width, 0.00045, 0.00006), WHITE)
    cap_y = y - travel / 2 + travel * value
    box((x, cap_y, 0.0045), (0.013, 0.0078, 0.007), KEYCAP, bevel=0.0014)
    box((x, cap_y, 0.0082), (0.011, 0.0007, 0.0003), DARK)            # index line
    for offset in (-0.0022, 0.0022):                                   # finger grooves
        box((x, cap_y + offset, 0.0081), (0.010, 0.00035, 0.0002), SLOT)
    label(title, (x, y + travel / 2 + 0.014, 0.00004), 0.0042, WHITE)


def led_button(x, y, title, lit):
    box((x, y, 0.003), (0.017, 0.011, 0.006), KEYCAP, bevel=0.0014)
    label(title, (x, y, 0.0061), 0.0036, DARK)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.0016, location=(x - 0.013, y, 0.0007))
    finish(bpy.context.object, LED_ON if lit else LED_OFF)
    if lit:
        light((x - 0.013, y, 0.004), LED_GLOW, 0.002, colour=(1.0, 0.16, 0.06), kind="POINT")


def screw(x, y, turn):
    cylinder((x, y, 0.0005), 0.0032, 0.001, STEEL, bevel=0.0003)
    box((x, y, 0.00095), (0.0052, 0.0007, 0.0004), SLOT, turn=turn)


# ---- the sample ----
bpy.ops.mesh.primitive_plane_add(size=1.0, location=(0, 0, 0))
panel = bpy.context.object
panel.scale = (0.34, 0.19, 1.0)
bpy.ops.object.transform_apply(scale=True)
panel.data.materials.append(PANEL)

box((-0.062, 0.058, 0.0001), (0.150, 0.0085, 0.0002), PAINTS["amber"])
label("R E V E R B", (-0.062, 0.058, 0.00025), 0.0048, DARK)
box((0.085, 0.058, 0.0001), (0.118, 0.0085, 0.0002), PAINTS["teal"])
label("F I L T E R", (0.085, 0.058, 0.00025), 0.0048, DARK)

knob(-0.115, -0.005, "amber", 0.62, "SIZE")
knob(-0.062, -0.005, "amber", 0.30, "DAMP")
knob(-0.009, -0.005, "amber", 0.85, "COLOR")
fader(0.050, -0.008, 0.070, 0.65, "FREQ")
fader(0.082, -0.008, 0.070, 0.25, "RES")
led_button(0.128, 0.008, "12 dB", False)
led_button(0.128, -0.012, "24 dB", True)
label("SLOPE", (0.124, 0.041, 0.00004), 0.0042, WHITE)
screw(-0.141, -0.064, 0.5)
screw(0.141, -0.064, 2.1)

# ---- camera: straight above, no perspective, as the plugin window will show it ----
bpy.ops.object.camera_add(location=(0, 0, 0.6))
camera = bpy.context.object
camera.data.type = "ORTHO"
camera.data.ortho_scale = 0.30
scene.camera = camera

light((0.0, 0.05, 0.55), SOFT_WATTS, 0.55)                               # soft light from above
light((-0.42, 0.34, 0.20), KEY_WATTS, 0.10, colour=(1.0, 0.96, 0.90))    # warm raking key, upper left
light((0.36, -0.30, 0.16), RIM_WATTS, 0.12, colour=(0.85, 0.92, 1.0))    # cool rim, lower right

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
scene.render.resolution_x = 1600
scene.render.resolution_y = 800
scene.render.filepath = OUT
os.makedirs(os.path.dirname(OUT), exist_ok=True)
bpy.ops.render.render(write_still=True)

# ---- photographic finish: grain and vignette, applied to the saved picture ----
if FILM_GRAIN > 0.0 or VIGNETTE > 0.0:
    try:
        image = bpy.data.images.load(OUT)
        width, height = image.size
        pixels = np.empty(width * height * 4, dtype=np.float32)
        image.pixels.foreach_get(pixels)
        pixels = pixels.reshape(height, width, 4)
        colour = pixels[:, :, :3]
        yy, xx = np.mgrid[0:height, 0:width]
        distance = ((xx / width - 0.5) ** 2 + ((yy / height - 0.5) * height / width) ** 2) / 0.3125
        colour *= (1.0 - VIGNETTE * distance)[:, :, None]
        random = np.random.default_rng(106)
        luminance = random.normal(0.0, FILM_GRAIN, (height, width, 1))
        tint = random.normal(0.0, FILM_GRAIN * 0.3, (height, width, 3))
        brightness = colour.mean(axis=2, keepdims=True)
        colour += (luminance + tint) * (0.55 + 0.9 * np.sqrt(np.clip(brightness, 0.0, 1.0)))  # grain shows more in the lights
        pixels[:, :, :3] = np.clip(colour, 0.0, 1.0)
        image.pixels.foreach_set(pixels.ravel())
        image.filepath_raw = OUT
        image.file_format = "PNG"
        image.save()
    except Exception as error:
        print("Photographic finish skipped:", error)
print("Orbit preview written to", OUT)
