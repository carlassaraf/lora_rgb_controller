NUM_LEDS = 100

def write_frame(filename, pixels):
    assert len(pixels) == NUM_LEDS
    data = bytearray()
    for r, g, b in pixels:
        data += bytes([b, r, g])  # BRG
    with open(filename, "wb") as f:
        f.write(data)
    print(f"{filename}: {len(data)} bytes")

import colorsys
import math

# FRAME 1: cian → verde
frame = [(0, 200, int(200*(NUM_LEDS-1-i)/(NUM_LEDS-1))) for i in range(NUM_LEDS)]
write_frame("FRAME001.BIN", frame)

# FRAME 2: rojo gradiente
frame = [(int(10+190*i/(NUM_LEDS-1)), 0, 0) for i in range(NUM_LEDS)]
write_frame("FRAME002.BIN", frame)

# FRAME 3: arcoiris
frame = []
for i in range(NUM_LEDS):
    r,g,b = colorsys.hsv_to_rgb(i/NUM_LEDS,1,0.8)
    frame.append((int(r*255),int(g*255),int(b*255)))
write_frame("FRAME003.BIN", frame)

# FRAME 4: todo rojo
write_frame("FRAME004.BIN", [(255,0,0)]*NUM_LEDS)

# FRAME 5: todo verde
write_frame("FRAME005.BIN", [(0,255,0)]*NUM_LEDS)

# FRAME 6: todo azul
write_frame("FRAME006.BIN", [(0,0,255)]*NUM_LEDS)

# FRAME 7: alternado rojo/azul
frame = [(255,0,0) if i%2==0 else (0,0,255) for i in range(NUM_LEDS)]
write_frame("FRAME007.BIN", frame)

# FRAME 8: alternado verde/negro
frame = [(0,255,0) if i%2==0 else (0,0,0) for i in range(NUM_LEDS)]
write_frame("FRAME008.BIN", frame)

# FRAME 9: gradiente negro → blanco
frame = []
for i in range(NUM_LEDS):
    v = int(255*i/(NUM_LEDS-1))
    frame.append((v,v,v))
write_frame("FRAME009.BIN", frame)

# FRAME 10: blanco → negro
frame = []
for i in range(NUM_LEDS):
    v = int(255*(NUM_LEDS-1-i)/(NUM_LEDS-1))
    frame.append((v,v,v))
write_frame("FRAME010.BIN", frame)

# FRAME 11: onda seno roja
frame = []
for i in range(NUM_LEDS):
    r = int((math.sin(i*0.2)+1)*127)
    frame.append((r,0,0))
write_frame("FRAME011.BIN", frame)

# FRAME 12: onda seno azul
frame = []
for i in range(NUM_LEDS):
    b = int((math.sin(i*0.2)+1)*127)
    frame.append((0,0,b))
write_frame("FRAME012.BIN", frame)

# FRAME 13: onda RGB desfasada
frame = []
for i in range(NUM_LEDS):
    r = int((math.sin(i*0.2)+1)*127)
    g = int((math.sin(i*0.2+2)+1)*127)
    b = int((math.sin(i*0.2+4)+1)*127)
    frame.append((r,g,b))
write_frame("FRAME013.BIN", frame)

# FRAME 14: mitad rojo mitad verde
frame = [(255,0,0) if i<NUM_LEDS//2 else (0,255,0) for i in range(NUM_LEDS)]
write_frame("FRAME014.BIN", frame)

# FRAME 15: tres zonas RGB
frame = []
for i in range(NUM_LEDS):
    if i < NUM_LEDS/3:
        frame.append((255,0,0))
    elif i < 2*NUM_LEDS/3:
        frame.append((0,255,0))
    else:
        frame.append((0,0,255))
write_frame("FRAME015.BIN", frame)

# FRAME 16: gradiente rojo → azul
frame = []
for i in range(NUM_LEDS):
    t = i/(NUM_LEDS-1)
    frame.append((int(255*(1-t)),0,int(255*t)))
write_frame("FRAME016.BIN", frame)

# FRAME 17: gradiente verde → magenta
frame = []
for i in range(NUM_LEDS):
    t = i/(NUM_LEDS-1)
    frame.append((int(255*t),int(255*(1-t)),int(255*t)))
write_frame("FRAME017.BIN", frame)

# FRAME 18: “punto brillante” en el centro
frame = []
for i in range(NUM_LEDS):
    d = abs(i - NUM_LEDS//2)
    v = max(0, 255 - d*10)
    frame.append((v,v,v))
write_frame("FRAME018.BIN", frame)

# FRAME 19: ruido random
import random
frame = [(random.randint(0,255), random.randint(0,255), random.randint(0,255)) for _ in range(NUM_LEDS)]
write_frame("FRAME019.BIN", frame)

# FRAME 20: arcoiris invertido
frame = []
for i in range(NUM_LEDS):
    r,g,b = colorsys.hsv_to_rgb(1 - i/NUM_LEDS,1,0.8)
    frame.append((int(r*255),int(g*255),int(b*255)))
write_frame("FRAME020.BIN", frame)

# FRAME 21: bandera de Argentina
frame = []
CELESTE = (75, 180, 255)
BLANCO  = (255, 255, 255)
SOL     = (255, 200, 0)
center = NUM_LEDS // 2
for i in range(NUM_LEDS):
    if i < NUM_LEDS // 3:
        frame.append(CELESTE)
    elif i < 2 * NUM_LEDS // 3:
        # pequeño sol en el centro
        if abs(i - center) < 3:
            frame.append(SOL)
        else:
            frame.append(BLANCO)
    else:
        frame.append(CELESTE)

write_frame("FRAME021.BIN", frame)

print("Listo: 21 frames generados.")