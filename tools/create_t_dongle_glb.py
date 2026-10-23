"""Photo-guided presentation model. Run: python tools/create_t_dongle_glb.py

Requires bpy (Blender Python). Internal dimensions inferred from photographs;
58.6 x 17.9 x 8.7 mm refers to the capped device. Not manufacturing CAD.
"""
import json
import math
import struct
import sys
from pathlib import Path

import bpy
from mathutils import Vector

OUT = Path(__file__).resolve().parents[1] / 'docs/models'
OUT.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'
# Build in millimetres, scale geometry to metres before export/render.


def material(name, color, metal=0, rough=.4, alpha=1):
    m = bpy.data.materials.new(name)
    m.diffuse_color = (*color, alpha)
    m.use_nodes = True
    bs = m.node_tree.nodes.get('Principled BSDF')
    bs.inputs['Base Color'].default_value = (*color, alpha)
    bs.inputs['Metallic'].default_value = metal
    bs.inputs['Roughness'].default_value = rough
    bs.inputs['Alpha'].default_value = alpha
    return m


clear = material('Clear polycarbonate / presentation transparency', (.65,.70,.68), rough=.12, alpha=.028)
rim = material('Polished moulded edges', (.65,.7,.67), rough=.2, alpha=.16)
green = material('Green solder mask', (.025,.22,.075), rough=.48)
trace = material('PCB copper under solder mask', (.04,.30,.11), metal=.25)
black = material('Matte IC encapsulation', (.016,.018,.022), rough=.48)
glass = material('Black LCD glass', (.004,.006,.009), rough=.19)
silver = material('Brushed nickel USB shell', (.58,.62,.66), metal=.82, rough=.29)
solder = material('Solder joints', (.66,.68,.67), metal=.8, rough=.24)
gold = material('Gold contacts', (.72,.44,.1), metal=.78, rough=.27)
white = material('White PCB silkscreen', (.8,.84,.72), rough=.65)
blue = material('Blue connector insulator', (.014,.10,.28), rough=.4)
tan = material('Ceramic capacitors', (.42,.28,.13))
red = material('Chip antenna ceramic', (.33,.035,.024))
flex = material('Amber display flex', (.45,.19,.03), rough=.4)


def finish(obj, name, mat, bevel=0):
    obj.name = name
    if mat:
        obj.data.materials.append(mat)
    if bevel:
        mod = obj.modifiers.new('Manufactured edge radius', 'BEVEL')
        mod.width = bevel
        mod.segments = 3
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.modifier_apply(modifier=mod.name)
        mod = obj.modifiers.new('Weighted face normals', 'WEIGHTED_NORMAL')
        bpy.ops.object.modifier_apply(modifier=mod.name)
    return obj


def box(name, center, size, mat, bevel=.04):
    bpy.ops.mesh.primitive_cube_add(size=1, location=center)
    obj = bpy.context.object
    obj.dimensions = size
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return finish(obj, name, mat, bevel)


def cylinder(name, center, radius, depth, mat):
    bpy.ops.mesh.primitive_cylinder_add(vertices=64, radius=radius, depth=depth, location=center)
    return finish(bpy.context.object, name, mat, .035)


def cut(obj, cutter):
    bpy.context.view_layer.objects.active = obj
    mod = obj.modifiers.new('Real opening', 'BOOLEAN')
    mod.operation = 'DIFFERENCE'
    mod.object = cutter
    bpy.ops.object.modifier_apply(modifier=mod.name)
    bpy.data.objects.remove(cutter, do_unlink=True)


def outline(width, bottom, top, radius):
    pts=[]
    lo,hi = radius if isinstance(radius,tuple) else (radius,radius)
    for x,y,start,r in [(width/2-lo,bottom+lo,-90,lo),(width/2-hi,top-hi,0,hi),(-width/2+hi,top-hi,90,hi),(-width/2+lo,bottom+lo,180,lo)]:
        for i in range(17):
            a=math.radians(start+i*90/16)
            pts.append((x+r*math.cos(a),y+r*math.sin(a)))
    return pts


def prism(name, width, bottom, top, z0, z1, radius, mat):
    ring=outline(width,bottom,top,radius)
    n=len(ring)
    verts=[(x,y,z) for z in (z0,z1) for x,y in ring]
    faces=[tuple(reversed(range(n))),tuple(range(n,2*n))]
    faces += [(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
    mesh=bpy.data.meshes.new(name)
    mesh.from_pydata(verts,[],faces)
    mesh.update()
    obj=bpy.data.objects.new(name,mesh)
    scene.collection.objects.link(obj)
    return finish(obj,name,mat,.1)


def line(name, coords, radius, mat):
    c=bpy.data.curves.new(name,'CURVE')
    c.dimensions='3D'
    c.bevel_depth=radius
    c.bevel_resolution=2
    p=c.splines.new('POLY')
    p.points.add(len(coords)-1)
    for point,co in zip(p.points,coords):
        point.co=(*co,1)
    obj=bpy.data.objects.new(name,c)
    scene.collection.objects.link(obj)
    obj.data.materials.append(mat)
    return obj


font=bpy.data.fonts.load('C:/Windows/Fonts/arialbd.ttf')
def text(name, body, pos, size, mat, back=False, angle=0):
    c=bpy.data.curves.new(name,'FONT')
    c.body=body
    c.font=font
    c.size=size
    c.align_x='CENTER'
    c.align_y='CENTER'
    c.resolution_u=6
    obj=bpy.data.objects.new(name,c)
    scene.collection.objects.link(obj)
    obj.location=pos
    obj.rotation_euler=(math.pi if back else 0,0,angle)
    obj.data.materials.append(mat)
    return obj


# Body is hollow. Split lid and lower tray preserve the visible mould seam.
shells=[]
for name,z0,z1 in [('Front clear lid',.12,4.35),('Rear clear tray',-4.35,-.12)]:
    obj=prism(name,17.9,0,43.6,z0,z1,(1.2,8.9),clear)
    inner=prism('temporary interior',16.65,.45,42.95,-3.85,3.85,(.8,8.2),None)
    cut(obj,inner)
    cut(obj,box('temporary USB mouth',(0,0,0),(12.8,2,5.5),None))
    cut(obj,cylinder('temporary lanyard opening',(0,40.1,0),1.5,12,None))
    shells.append(obj)
for z in (-.13,.13):
    pts=outline(17.65,.1,43.5,(1.1,8.8))
    line('Housing seam',[(x,y,z) for x,y in pts+[pts[0]]],.08,rim)
for x in (-7.65,7.65):
    box('PCB support rail',(x,18,0),(.55,34,1.6),rim,.15)
    for y in (4,32):
        box('Snap fit lug',(x,y,.8),(.85,1.5,.8),rim,.18)

# PCB: display on front; CPU, memory and switch on rear.
prism('FR4 main PCB',14.7,1.3,37.8,-.4,.4,1,green)
box('LCD steel backing',(0,20,1.45),(13.1,29.9,.5),silver,.18)
box('LCD bezel',(0,20,1.9),(13.05,29.75,.65),black,.23)
box('LCD active glass',(0,20,2.25),(10.8,25.8,.16),glass,.1)
box('Display flex cable',(0,4.6,.95),(10,2.2,.2),flex)
for x in range(18):
    box('Display flex conductor',(-4.4+x*.52,4.6,1.06),(.14,1.9,.025),gold,.005)
text('Board maker','LILYGO',(0,36.2,.42),1.55,white)
colors=[(.95,.2,.22),(.6,.2,.9),(.2,.5,1),(.1,.85,.6),(.9,.76,.14),(.97,.55,.1)]
# Text lies along the long screen axis, as in the supplied front photograph.
for i,(ch,col) in enumerate(zip('LILYGO',colors)):
    m=material('LCD logo '+str(i),col,rough=.35)
    m.node_tree.nodes.get('Principled BSDF').inputs['Emission Color'].default_value=(*col,1)
    m.node_tree.nodes.get('Principled BSDF').inputs['Emission Strength'].default_value=.25
    letter=text('Screen letter '+str(i),ch,(0,29.4-i*3.7,2.345),4.8,m,angle=-math.pi/2)
    letter.scale.y=1.7

box('ESP32-S3 QFN',(0,25.8,-.97),(7.2,7.2,1.12),black,.12)
text('ESP32 marking','ESP32-S3',(0,26.1,-1.542),.76,white,True)
text('ESP32 logo','ESPRESSIF',(0,24.9,-1.542),.52,white,True)
for side in (-1,1):
    for i in range(14):
        t=-3.12+i*.48
        box('QFN solder terminal',(side*3.75,25.8+t,-.53),(.46,.22,.22),solder,.025)
        box('QFN solder terminal',(t,25.8+side*3.75,-.53),(.22,.46,.22),solder,.025)
for name,x,y,w,h in [('Flash',0,17.8,3.8,4.6),('Regulator',-4.5,7,1.6,2.3)]:
    box(name,(x,y,-.85),(w,h,.85),black,.1)
    for side in (-1,1):
        for i in range(4):
            box(name+' lead',(x+side*(w/2+.3),y-h*.35+i*h*.23,-.64),(.6,.35,.25),solder)
text('Flash marking','FLASH',(0,17.8,-1.29),.7,white,True)
box('Ceramic antenna',(0,35.55,-.95),(4.7,2.1,1.05),red,.14)
for x in (-2.4,2.4):
    box('Antenna solder',(x,35.55,-.74),(.7,2.15,.7),solder)
box('Crystal oscillator',(4.65,31,-.85),(2.5,3.2,.85),silver,.22)
box('BOOT tactile switch frame',(0,9,-.98),(5.5,5.5,1.15),silver,.28)
cylinder('BOOT switch dark gasket',(0,9,-1.68),2.05,.35,black)
cylinder('BOOT brass actuator',(0,9,-1.95),1.48,.35,gold)
text('BOOT silkscreen','BOOT',(0,5.35,-.42),.85,white,True)
for x in (-2.8,2.8):
    for y in (7.1,10.9):
        box('Switch solder feet',(x,y,-.55),(.8,.9,.3),solder)
# Rear moulded button access ring, with a real through hole.
cut(shells[1],cylinder('temporary switch opening',(0,9,-4),2.25,3,None))
for y in (14,20.5,30.5,33):
    for x in (-5.5,-4.1,4.1,5.5):
        box('SMD ceramic',(x,y,-.7),(.8,1.25,.5),tan,.06)
        for dy in (-.56,.56):
            box('SMD solder end',(x,y+dy,-.68),(.87,.27,.5),solder,.025)
for side in (-1,1):
    for i in range(9):
        y=12+i*2.25
        line('PCB routed trace',[(side*6.6,y,-.415),(side*6,y+.7,-.415),(side*4.1,y+.7,-.415)],.035,trace)
        cylinder('Gold test pad',(side*6.55,y,-.435),.18,.045,gold)
text('Revision label','T-Dongle-S3',(0,38,-.44),.8,white,True)
text('RGB label','RGB LED',(5.9,18,-.43),.7,white,True,math.pi/2)
box('RGB LED body',(5.7,13.1,-.72),(1.4,1.4,.55),white,.13)
box('RGB LED die',(5.7,13.1,-1.02),(.55,.55,.07),gold)

# USB shell has wall thickness, an open mouth and punched retention holes.
usb=box('USB-A folded metal shell',(0,-5.5,0),(12,12,4.5),silver,.16)
cut(usb,box('temporary shell bore',(0,-6,0),(11.45,14,3.95),None,.12))
for z in (-2.25,2.25):
    for x in (-2.8,2.8):
        cut(usb,box('temporary retention opening',(x,-6.5,z),(2.15,2.1,1),None,.1))
        box('USB stamped spring',(x,-3.8,z),(.9,.75,.12),silver,.04)
box('USB blue tongue',(0,-5.7,-.85),(10.45,11.4,1.25),blue,.12)
for x in (-3.5,-1.15,1.15,3.5):
    box('USB gold contact',(x,-8.1,-.19),(1.05,5.2,.1),gold,.04)
line('USB folded seam',[(0,-11.4,2.27),(0,.3,2.27)],.025,black)
box('TF slot mouth',(0,-10.9,-1.65),(9.3,.5,.45),black,.05)
for i in range(8):
    box('TF spring contact',(-3.7+i*1.05,-10.7,-1.65),(.23,.65,.15),gold,.02)

# Separate cap, positioned beside the device for the presentation view.
before=set(bpy.data.objects)
cap=prism('Removable transparent USB cap',17.9,-15,0,-4.35,4.35,(8.9,.8),clear)
cut(cap,prism('temporary cap cavity',16.55,-14.25,2,-3.75,3.75,(8.2,.6),None))
pts=outline(17.45,-14.8,-.25,(8.7,.7))
line('Cap mould seam',[(x,y,0) for x,y in pts+[pts[0]]],.075,rim)
box('Cap latch',(0,-.8,4.12),(2.4,1.1,.25),rim,.12)
cap_parts=list(set(bpy.data.objects)-before)
for obj in cap_parts:
    obj.location.x += 24
    obj.location.y += 8

# Freeze curves/text and export through Blender's glTF writer.
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.convert(target='MESH')
for obj in list(scene.objects):
    if obj.type=='MESH':
        obj.data.transform(obj.matrix_world)
        obj.matrix_world.identity()
        for v in obj.data.vertices:
            v.co *= .001
        obj.data.update()
model_objects=list(scene.objects)
cap_parts=[obj for obj in model_objects if obj.name.startswith(('Removable transparent','Cap mould','Cap latch'))]


def export(name):
    bpy.ops.export_scene.gltf(filepath=str(OUT/name),export_format='GLB',export_apply=True,export_cameras=False,export_lights=False)
    data=(OUT/name).read_bytes()
    magic,version,length=struct.unpack_from('<4sII',data)
    assert (magic,version,length)==(b'glTF',2,len(data))
    n=struct.unpack_from('<I',data,12)[0]
    doc=json.loads(data[20:20+n])
    assert len(doc['meshes'])>100
    assert all(a['count']>0 for a in doc['accessors'])
    binary_start=28+n
    for view in doc['bufferViews']:
        assert view.get('byteOffset',0)+view['byteLength'] <= doc['buffers'][0]['byteLength']
    for mesh in doc['meshes']:
        for primitive in mesh['primitives']:
            pos=doc['accessors'][primitive['attributes']['POSITION']]
            idx=doc['accessors'][primitive['indices']]
            view=doc['bufferViews'][idx['bufferView']]
            fmt={5121:'B',5123:'H',5125:'I'}[idx['componentType']]
            offset=binary_start+view.get('byteOffset',0)+idx.get('byteOffset',0)
            indices=struct.unpack_from('<'+str(idx['count'])+fmt,data,offset)
            assert idx['count']%3==0 and max(indices)<pos['count']
    assert any(m.get('alphaMode')=='BLEND' for m in doc['materials'])
    print(f'CHECKED {name}: {len(doc["meshes"])} meshes, {len(data):,} bytes',flush=True)


export('lilygo-t-dongle-s3-3d-exploded.glb')
for obj in cap_parts:
    obj.location.x-=.024
    obj.location.y-=.008
bpy.context.view_layer.update()
points=[obj.matrix_world@v.co for obj in model_objects for v in obj.data.vertices]
dimensions=[max(p[i] for p in points)-min(p[i] for p in points) for i in range(3)]
assert all(abs(actual-expected)<.0002 for actual,expected in zip(dimensions,(.0179,.0586,.0087))),dimensions
export('lilygo-t-dongle-s3-3d-assembled.glb')
for obj in cap_parts:
    obj.location.x+=.024
    obj.location.y+=.008

if '--no-render' in sys.argv:
    print('DONE: GLB exports and geometry checks.',flush=True)
    sys.exit(0)

# Render the actual exported GLB, proving the import path as well as appearance.
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
bpy.ops.import_scene.gltf(filepath=str(OUT/'lilygo-t-dongle-s3-3d-exploded.glb'))
scene.render.engine='CYCLES'
scene.cycles.samples=24
scene.cycles.use_denoising=True
scene.render.resolution_x=1400
scene.render.resolution_y=1000
scene.render.resolution_percentage=100
scene.world=bpy.data.worlds.new('Studio world')
scene.world.use_nodes=True
scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.65,.7,.8,1)
scene.world.node_tree.nodes['Background'].inputs[1].default_value=.5
scene.render.film_transparent=False
scene.view_settings.view_transform='AgX'
def aim(obj,point):
    obj.rotation_euler=(Vector(point)-obj.location).to_track_quat('-Z','Y').to_euler()
for pos,power,size in [((.025,.035,.08),.045,.07),((-.04,.01,.04),.03,.05),((0,.02,-.06),.035,.05)]:
    bpy.ops.object.light_add(type='AREA',location=pos)
    lamp=bpy.context.object
    lamp.data.energy=power
    lamp.data.shape='DISK'
    lamp.data.size=size
    aim(lamp,(0,.015,0))
bpy.ops.object.camera_add(location=(.065,-.055,.105))
camera=bpy.context.object
scene.camera=camera
camera.data.type='ORTHO'
camera.data.ortho_scale=.086
camera.data.clip_start=.001
camera.data.clip_end=10
aim(camera,(.006,.014,0))
for name,pos in [('front',(.05,-.045,.105)),('back',(-.045,-.035,-.105))]:
    camera.location=pos
    aim(camera,(.006,.014,0))
    scene.render.filepath=str(OUT/f'lilygo-t-dongle-s3-render-{name}.png')
    bpy.ops.render.render(write_still=True)
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'lilygo-t-dongle-s3-3d-source.blend'))
print('DONE: GLB variants, editable Blender file, front/back previews.',flush=True)
