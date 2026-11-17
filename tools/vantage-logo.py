from PIL import Image, ImageDraw, ImageFilter
S=4; W=1024*S
violet=(118,92,168,255); grey=(200,206,212,255); dark=(29,30,31,255); edge=(58,58,60,255)
im=Image.new('RGBA',(W,W),(0,0,0,0))
def r(v): return int(v*S)
sh=Image.new('RGBA',(W,W),(0,0,0,0)); ImageDraw.Draw(sh).rounded_rectangle([r(102),r(114),r(922),r(934)],radius=r(176),fill=(0,0,0,70))
im=Image.alpha_composite(im,sh.filter(ImageFilter.GaussianBlur(r(6))))
box=[r(102),r(102),r(920),r(920)]
tile=Image.new('RGBA',(W,W),(0,0,0,0)); t=ImageDraw.Draw(tile)
t.rectangle([0,0,W,r(730)],fill=dark); t.rectangle([0,r(730),W,r(826)],fill=grey); t.rectangle([0,r(826),W,W],fill=violet)
mask=Image.new('L',(W,W),0); ImageDraw.Draw(mask).rounded_rectangle(box,radius=r(176),fill=255)
im.paste(tile,(0,0),mask); d=ImageDraw.Draw(im)
d.rounded_rectangle(box,radius=r(176),outline=edge,width=r(5))
# the V: a heavy chevron in light grey, same height as the 8, with a violet point at its foot (the vantage point)
top,bot=258,582; xl,xr=205,555; st=78
d.polygon([(r(xl),r(top)),(r(xl+st*1.15),r(top)),(r((xl+xr)/2),r(bot-st*1.25)),(r(xr-st*1.15),r(top)),(r(xr),r(top)),(r((xl+xr)/2+st*.62),r(bot)),(r((xl+xr)/2-st*.62),r(bot))],fill=grey)
cx=(xl+xr)/2; d.ellipse([r(cx-44),r(bot-6-44),r(cx+44),r(bot-6+44)],fill=dark); d.ellipse([r(cx-28),r(bot-6-28),r(cx+28),r(bot-6+28)],fill=violet)
x0,x1,mid,stk=585,748,412,54
d.rounded_rectangle([r(x0+8),r(top),r(x1-8),r(mid+stk*.5)],radius=r(72),fill=violet)
d.rounded_rectangle([r(x0),r(mid-stk*.5),r(x1),r(bot)],radius=r(80),fill=violet)
d.rounded_rectangle([r(x0+8+stk),r(top+stk),r(x1-8-stk),r(mid-stk*.5)],radius=r(28),fill=dark)
d.rounded_rectangle([r(x0+stk),r(mid+stk*.5),r(x1-stk),r(bot-stk)],radius=r(32),fill=dark)
im.resize((1024,1024),Image.LANCZOS).save('vantage-icon.png')
im.resize((256,256),Image.LANCZOS).save('preview.png')
