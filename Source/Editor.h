#pragma once
#include <BinaryData.h>
// Orbit front panel: an original layout in the spirit of early-80s programmable polysynths.
// The vector panel uses a fixed design surface, scaled as a whole
// when the plugin window is resized. Everything is on one face: a row of faders for the playing
// controls, and under it a row of knobs and switches for shapes, modulation and the second filter.

namespace orbitui {
 const Colour panel{0xff080909}, ink{0xfff0e5ca}, dim{0xffc1b99f}, amber{0xffd9772b}, teal{0xff7a5fae}; // Vantage: the playing sections in violet (the name "teal" is kept from Orbit)
 // Vantage: light steel grey section bars printed in near-black, beside the blue ones.
 const Colour steel{0xffc6ccd2}, steelInk{0xff15181b};
 const Colour neutral{0xffd9d3c3}, bright{0xffe6e0d1}, led{0xffedac62}, slot{0xff0b0c0d}, print{0xff141516};
 inline Font face(float size, bool bold = false) { return Font(FontOptions("DIN Condensed", size, bold ? Font::bold : Font::plain)); }
 inline Font legend(float size) { return Font(FontOptions("Helvetica Neue", size, Font::plain)); }
 inline Font mono(float size) { return Font(FontOptions("Menlo", size, Font::plain)); }
 inline String utf8(const char* text) { return String::fromUTF8(text); }
 inline void tint(TextButton& button, Colour colour) { button.setColour(TextButton::buttonColourId, colour); button.setColour(TextButton::buttonOnColourId, colour); }
 // "led" property on a button: 0 = plain cap, 1 = LED above the cap, 2 = LED left of the cap.
 inline int ledMode(Button& button) { return int(button.getProperties().getWithDefault("led", 0)); }
}

// Rendered artwork ("skin"). When every image is present in the Skin folder, the editor shows the rendered
// panel and controls instead of drawing them; when one is missing it falls back to the drawn interface.
// The images are produced by tools/orbit_render.py from the layout file the editor writes in the same folder.
struct OrbitSkin {
 Image background, knob, fader, led; std::array<Image, 3> index; // index line in amber, teal, neutral
 bool ready = false;
 static File folder() { return Orbit::presetRoot().getChildFile("Skin"); }
 // Where the installer puts the artwork for every user of the computer (/Library/Application Support/... on macOS).
 static File sharedFolder() { return File::getSpecialLocation(File::commonApplicationDataDirectory).getChildFile("Application Support").getChildFile("SF Instruments").getChildFile("Vantage").getChildFile("Skin"); }
 void load(const File& directory) {
  const auto get = [&directory](const char* name) { return ImageFileFormat::loadFrom(directory.getChildFile(name)); };
  background = get("background.jpg"); knob = get("knob.png"); fader = get("fader.png"); led = get("led.png");
  index[0] = get("index_amber.png"); index[1] = get("index_teal.png"); index[2] = get("index_neutral.png");
  ready = background.isValid() && knob.isValid() && fader.isValid() && led.isValid() && index[0].isValid() && index[1].isValid() && index[2].isValid();
 }
 const Image& indexFor(Colour colour) const { return colour == orbitui::amber ? index[0] : (colour == orbitui::teal ? index[1] : index[2]); }
 // Sizes of the sprites, in design pixels: the knob image is this many times the knob's diameter (room for
 // its shadow); the fader cap and the lit LED are drawn in squares of these sides.
 static constexpr float knobFrame = 1.6f, faderFrame = 48.f, ledFrame = 28.f;
 // The artwork is rendered from slightly in front of the panel, not straight above (TILT in orbit_render.py),
 // so the top of a knob is seen as a flattened circle, raised above the knob's centre.
 static constexpr float tiltCos = 1.f;      // cos(TILT): 1 = artwork seen from straight above
 static constexpr float topLift = 0.f;      // top of the knob sits on its centre when seen from above
};

// All controls are drawn at native resolution, so the new metal finish scales with the window.
class OrbitLook : public LookAndFeel_V4 {
public:
 OrbitLook(){using namespace orbitui;
  setColour(Label::textColourId,ink);setColour(TextButton::buttonColourId,neutral);
  setColour(ComboBox::backgroundColourId,Colours::transparentBlack);setColour(ComboBox::outlineColourId,Colours::transparentBlack);
  setColour(ComboBox::textColourId,led);setColour(ComboBox::arrowColourId,led);
  setColour(PopupMenu::backgroundColourId,Colour(0xff0e1010));setColour(PopupMenu::textColourId,ink);
  setColour(PopupMenu::headerTextColourId,led);setColour(PopupMenu::highlightedBackgroundColourId,amber);
  setColour(PopupMenu::highlightedTextColourId,ink);
 }
 // Orbit's own rendered knob, fader cap and LED (tools/orbit_render.py), embedded in the plug-in.
 Image knobSprite=ImageCache::getFromMemory(BinaryData::knob_png,BinaryData::knob_pngSize);
 Image faderSprite=ImageCache::getFromMemory(BinaryData::fader_png,BinaryData::fader_pngSize);
 Image ledSprite=ImageCache::getFromMemory(BinaryData::led_png,BinaryData::led_pngSize);
 std::array<Image,3> indexSprite{ImageCache::getFromMemory(BinaryData::index_amber_png,BinaryData::index_amber_pngSize),
  ImageCache::getFromMemory(BinaryData::index_teal_png,BinaryData::index_teal_pngSize),ImageCache::getFromMemory(BinaryData::index_neutral_png,BinaryData::index_neutral_pngSize)};
 const Image& indexFor(Colour colour) const {return colour==orbitui::amber?indexSprite[0]:(colour==orbitui::teal?indexSprite[1]:indexSprite[2]);}
 void drawLed(Graphics& g,Rectangle<float> spot,bool on,bool blue=false) const {
  const auto centre=spot.getCentre();
  if(on&&blue){ // OVERDUB: a blue LED
   for(int i=4;i>=1;--i){g.setColour(Colour(0xff3fa9ff).withAlpha(.09f));g.fillEllipse(Rectangle<float>(5.f+float(i)*3.f,5.f+float(i)*3.f).withCentre(centre));}
   g.setColour(Colour(0xff6cc4ff));g.fillEllipse(Rectangle<float>(5.5f,5.5f).withCentre(centre));g.setColour(Colours::white.withAlpha(.7f));g.fillEllipse(Rectangle<float>(2.f,2.f).withCentre(centre.translated(-.6f,-.6f)));return;}
  if(on&&ledSprite.isValid()){g.drawImage(ledSprite,Rectangle<float>(26.f,26.f).withCentre(centre));return;}
  const auto dome=Rectangle<float>(5.f,5.f).withCentre(centre);
  g.setColour(Colour(0xff050505));g.fillEllipse(dome.expanded(1.f));g.setColour(Colour(0xff3a120c));g.fillEllipse(dome);
  g.setColour(Colours::white.withAlpha(.12f));g.fillEllipse(dome.reduced(1.4f).translated(-.5f,-.6f));
 }
 Font getTextButtonFont(TextButton& button,int height) override{if(button.getProperties().contains("fontSize"))return orbitui::face(float(button.getProperties()["fontSize"]));return orbitui::face(jlimit(10.f,16.f,height*.65f));}
 Font getComboBoxFont(ComboBox&) override{return orbitui::mono(17.f);}
 Font getPopupMenuFont() override{return orbitui::face(17.f);}
 void drawButtonBackground(Graphics& g,Button& button,const Colour&,bool over,bool down) override {
  using namespace orbitui;auto area=button.getLocalBounds().toFloat();
  if(button.getProperties().contains("octaveRocker")){
   auto cap=area.reduced(2,0);g.setColour(Colours::black);g.fillRect(area);
   const bool upper=int(button.getProperties()["octaveRocker"])>0;
   ColourGradient rocker(Colour(0xff555750),0,upper?cap.getY():cap.getBottom(),Colour(0xff151716),0,upper?cap.getBottom():cap.getY(),false);
   if(down)rocker=ColourGradient(Colour(0xff181a18),0,cap.getY(),Colour(0xff353830),0,cap.getBottom(),false);
   g.setGradientFill(rocker);g.fillRect(cap.reduced(1));g.setColour(Colours::white.withAlpha(over?.25f:.12f));g.drawRect(cap.reduced(1),.7f);return;
  }
  const bool lit=button.getToggleState()||down;
  const bool blue=button.getProperties().contains("ledBlue");
  if(button.getProperties().contains("legendAbove")){area.removeFromTop(16);drawLed(g,area.removeFromTop(9),lit,blue);}
  else if(const int mode=ledMode(button);mode!=0)drawLed(g,mode==1?area.removeFromTop(12):area.removeFromLeft(12),lit);
  const auto cap=area.reduced(1.5f).withTrimmedBottom(2).translated(0,down?1.f:0.f);
  const bool cream=button.getProperties().contains("cream");
  g.setColour(Colours::black.withAlpha(.85f));g.fillRoundedRectangle(cap.expanded(1.5f).translated(0,2),3.5f); // the well and the cap's shadow
  if(cream){ // memory and utility keys: light caps, lit from inside in amber when selected
   ColourGradient body(Colour(0xffefe9da),cap.getX(),cap.getY(),Colour(0xffb9b2a1),cap.getX(),cap.getBottom(),false);body.addColour(.18,Colour(0xffe2dccb));
   if(lit){body=ColourGradient(Colour(0xffffc46a),cap.getX(),cap.getY(),Colour(0xffc56e1c),cap.getX(),cap.getBottom(),false);
    for(int i=1;i<=4;++i){g.setColour(Colour(0xffffa42a).withAlpha(.05f));g.fillRoundedRectangle(cap.expanded(float(i)*1.5f),3.f+float(i));}}
   g.setGradientFill(body);g.fillRoundedRectangle(cap,3);
   g.setColour(Colours::white.withAlpha(.55f));g.drawHorizontalLine(int(cap.getY())+1,cap.getX()+2,cap.getRight()-2);
   g.setColour(Colour(0xff3c372c));g.drawRoundedRectangle(cap,3,.7f);
  } else { // playing keys: dark glossy caps with a light bar set in the top edge, which comes on when the key is on
   ColourGradient body(Colour(0xff34373a),cap.getX(),cap.getY(),Colour(0xff0a0b0c),cap.getX(),cap.getBottom(),false);body.addColour(.15,Colour(0xff222427));body.addColour(.85,Colour(0xff0f1011));
   g.setGradientFill(body);g.fillRoundedRectangle(cap,3);
   g.setColour(Colours::white.withAlpha(.16f));g.drawHorizontalLine(int(cap.getY())+1,cap.getX()+2,cap.getRight()-2);
   g.setColour(Colours::black);g.drawRoundedRectangle(cap,3,.8f);
   if(cap.getHeight()>=16.f){
    const auto bar=Rectangle<float>(jmax(8.f,cap.getWidth()*.44f),3.f).withCentre({cap.getCentreX(),cap.getY()+5.f});
    if(lit){const Colour glow(blue?0xff3fa9ff:0xffffa42a);for(int i=1;i<=5;++i){g.setColour(glow.withAlpha(.075f));g.fillRoundedRectangle(bar.expanded(float(i)*1.7f),float(i)*1.7f);}
     g.setColour(Colour(blue?0xff6cc4ff:0xffffb84a));g.fillRoundedRectangle(bar,1.5f);g.setColour(Colours::white.withAlpha(.55f));g.fillRoundedRectangle(bar.reduced(2.f,.8f),1.f);}
    else{g.setColour(Colour(0xff2b1608));g.fillRoundedRectangle(bar,1.5f);g.setColour(Colours::black.withAlpha(.5f));g.drawHorizontalLine(int(bar.getY()),bar.getX(),bar.getRight());}
   } else if(lit){g.setColour(Colour(blue?0xff3fa9ff:0xffffa42a).withAlpha(.35f));g.fillRoundedRectangle(cap,3);}
  }
  if(over){g.setColour(Colours::white.withAlpha(.08f));g.fillRoundedRectangle(cap,3);}
 }
 void drawButtonText(Graphics& g,TextButton& button,bool,bool down) override {
  auto area=button.getLocalBounds();
  if(button.getProperties().contains("legendAbove")){
   g.setColour(orbitui::ink);g.setFont(orbitui::face(11.f));
   g.drawFittedText(button.getButtonText(),area.withHeight(14),Justification::centred,1,.8f);return;
  }
  const int mode=orbitui::ledMode(button);if(mode==1)area.removeFromTop(12);else if(mode==2)area.removeFromLeft(12);
  const bool cream=button.getProperties().contains("cream"),rocker=button.getProperties().contains("octaveRocker");
  if(!cream&&!rocker&&area.getHeight()>=20)area.removeFromTop(5); // under the light bar
  if(down)area.translate(0,1);g.setFont(getTextButtonFont(button,area.getHeight()));g.setColour(rocker||!cream?orbitui::ink:orbitui::print);
  g.drawFittedText(button.getButtonText(),area.reduced(3,2),Justification::centred,1,.75f);
 }
 void drawLinearSlider(Graphics& g,int x,int y,int w,int h,float pos,float,float,Slider::SliderStyle style,Slider& slider) override {
  using namespace orbitui;
  if(style==Slider::LinearHorizontal){
   Rectangle<float> well(float(x),float(y+h/2-11),float(w),22);g.setColour(Colour(0xff020303));g.fillRoundedRectangle(well,3);
   g.setColour(Colour(0xff363936));g.drawRoundedRectangle(well,3,1);
   Rectangle<float> cap(pos-12,well.getY()+2,24,18);g.setGradientFill(ColourGradient(Colour(0xff333633),cap.getX(),cap.getY(),Colour(0xff060707),cap.getX(),cap.getBottom(),false));g.fillRoundedRectangle(cap,2);
   g.setColour(ink);g.fillRect(pos-1,cap.getY()+2,2.f,cap.getHeight()-4);return;
  }
  if(slider.getProperties().contains("lever")){ // a chrome lever riding in a slot on a brushed plate, one stop per position
   const float cx=float(x)+float(w)*.5f;const auto plate=Rectangle<float>(cx-9,float(y)-4,18,float(h)+8);
   g.setGradientFill(ColourGradient(Colour(0xff3a3c3e),plate.getX(),0,Colour(0xff1c1d1e),plate.getRight(),0,false));g.fillRoundedRectangle(plate,3);
   g.setColour(Colour(0xff626466));g.drawRoundedRectangle(plate,3,.7f);
   g.setColour(Colour(0xff010101));g.fillRoundedRectangle(cx-2.5f,float(y)+2,5,float(h)-4,2.5f);           // the slot
   const int stops=int(slider.getProperties()["marks"]);for(int i=0;i<=stops;++i){const float yy=float(y)+float(h)*float(i)/float(jmax(1,stops));
    g.setColour(dim);g.fillRect(cx+11,yy-.6f,5.f,1.2f);}                                                       // stop marks beside the plate
   const auto bat=Rectangle<float>(cx-6,pos-9,12,18);                                                           // the lever head
   g.setColour(Colours::black.withAlpha(.5f));g.fillRoundedRectangle(bat.translated(1.5f,2.f),3);
   ColourGradient chrome(Colour(0xff6d7073),bat.getX(),0,Colour(0xff4a4d50),bat.getRight(),0,false);
   chrome.addColour(.35,Colour(0xffe9ecee));chrome.addColour(.55,Colour(0xffa9adb1));g.setGradientFill(chrome);g.fillRoundedRectangle(bat,3);
   g.setColour(Colours::white.withAlpha(.55f));g.drawHorizontalLine(int(bat.getY())+2,bat.getX()+2,bat.getRight()-2);
   g.setColour(Colours::black.withAlpha(.35f));g.drawRoundedRectangle(bat,3,.8f);
   g.setColour(Colours::black.withAlpha(.25f));for(float k:{-3.f,0.f,3.f})g.drawHorizontalLine(int(bat.getCentreY()+k),bat.getX()+2,bat.getRight()-2); // grip
   return;
  }
  if(slider.getProperties().contains("pitchWheel")){ // Orbit's bender: a dark knurled drum turning in a metal-framed well
   const auto well=Rectangle<float>(float(x+w/2-21),float(y),42.f,float(h));
   g.setColour(Colour(0xff2c2e30));g.fillRoundedRectangle(well.expanded(3.f),5.f);                     // metal frame
   g.setColour(Colour(0xff5c5d5e));g.drawRoundedRectangle(well.expanded(3.f),5.f,.7f);
   g.setColour(Colour(0xff010101));g.fillRoundedRectangle(well,4.f);                                     // the opening
   const auto drum=well.reduced(5.f,6.f);const float mid=drum.getCentreY();
   ColourGradient body(Colour(0xff08090a),0,drum.getY(),Colour(0xff08090a),0,drum.getBottom(),false);      // round: lit in the middle
   body.addColour(.5,Colour(0xff45484b));body.addColour(.3,Colour(0xff26282a));body.addColour(.7,Colour(0xff26282a));
   g.setGradientFill(body);g.fillRoundedRectangle(drum,3.f);
   {Graphics::ScopedSaveState clip(g);g.reduceClipRegion(drum.toNearestInt());
    const float pitch=5.f,shift=std::fmod(pos-mid+1000.f*pitch,pitch);                                    // the knurling turns with the drum
    for(float yy=drum.getY()-pitch+shift;yy<drum.getBottom()+pitch;yy+=pitch){
     const float light=std::cos(jlimit(-1.5f,1.5f,(yy-mid)/(drum.getHeight()*.5f)*1.5f));
     g.setColour(Colours::white.withAlpha(.13f*light));g.fillRect(drum.getX()+1.f,yy,drum.getWidth()-2.f,1.f);
     g.setColour(Colours::black.withAlpha(.6f));g.fillRect(drum.getX()+1.f,yy+1.2f,drum.getWidth()-2.f,1.f);}
    const float index=jlimit(drum.getY()+2.f,drum.getBottom()-3.f,pos);                                   // amber index line on the drum
    g.setColour(Colour(0xffffa42a).withAlpha(.25f));g.fillRect(drum.getX(),index-2.f,drum.getWidth(),4.f);
    g.setColour(Colour(0xffffb84a));g.fillRect(drum.getX()+1.f,index-1.f,drum.getWidth()-2.f,2.f);}
   g.setColour(Colours::black.withAlpha(.55f));g.drawHorizontalLine(int(drum.getY()),drum.getX(),drum.getRight());
   g.setColour(Colour(0xffd9d3c3));for(float side:{well.getX()-3.f,well.getRight()+1.f})g.fillRect(side,mid-.75f,2.f,1.5f); // centre marks on the frame
   return;
  }
  const float cx=float(x)+float(w)*.5f,top=float(y),length=float(h);
  const int marks=jmax(1,int(slider.getProperties().getWithDefault("marks",10)));
  // A ladder on each side: long bold marks at 0, 5 and 10, short dim ones between, joined by a thin rail.
  const float reach=jmin(17.5f,float(w)*.5f-1.f);
  for(float side:{-1.f,1.f}){g.setColour(dim.withAlpha(.55f));g.fillRect(cx+side*reach-.4f,top,.8f,length);}
  for(int i=0;i<=marks;++i){const float yy=top+length*float(i)/float(marks);const bool major=marks!=10||i%5==0;
   for(float side:{-1.f,1.f}){if(major){g.setColour(ink.withAlpha(.9f));g.fillRect(side<0?cx-reach:cx+6.5f,yy-.8f,reach-6.5f,1.6f);}
    else{g.setColour(dim.withAlpha(.7f));g.fillRect(side<0?cx-reach:cx+9.5f,yy-.45f,reach-9.5f,.9f);}}}
  g.setColour(Colour(0xff2c2e30));g.fillRoundedRectangle(cx-4.5f,top-9,9,length+18,2.5f);   // metal lip
  g.setColour(Colour(0xff58595a));g.drawRoundedRectangle(cx-4.5f,top-9,9,length+18,2.5f,.6f);
  g.setColour(Colour(0xff010101));g.fillRoundedRectangle(cx-2,top-7,4,length+14,1.5f);          // the slot
  if(faderSprite.isValid()){g.drawImage(faderSprite,Rectangle<float>(48.f,48.f).withCentre({cx,pos}));return;}
  Rectangle<float> cap(cx-13,pos-7,26,14);g.setColour(Colour(0xffe2dccb));g.fillRoundedRectangle(cap,2);g.setColour(Colours::black);g.fillRect(cap.getX()+2,pos-.9f,cap.getWidth()-4,1.8f);
 }
 void drawRotarySlider(Graphics& g,int x,int y,int w,int h,float value,float start,float end,Slider& slider) override {
  using namespace orbitui;
  const float diameter=jmin(float(slider.getProperties().getWithDefault("faceDiameter",34.f)),float(jmin(w,h))-8);
  const auto area=Rectangle<float>(float(x),float(y),float(w),float(h)).withSizeKeepingCentre(diameter,diameter);
  const auto centre=area.getCentre();const float radius=diameter*.5f,angle=start+value*(end-start);
  if(!slider.getProperties().contains("rangeSelector")&&!slider.getProperties().contains("shapeScale"))for(int i=0;i<=10;++i){float a=start+(end-start)*i/10.f;auto a1=centre+Point<float>(std::sin(a),-std::cos(a))*(radius+4);auto a2=centre+Point<float>(std::sin(a),-std::cos(a))*(radius+6);g.setColour(dim.withAlpha(.8f));g.drawLine({a1,a2},.8f);}
  if(slider.getProperties().contains("shapeScale")){ // continuous SHAPE: the four landmark waves printed around the knob
   const char* names[]={"TRI","SAW","SQR","PLS"};g.setFont(face(11.f));
   for(int i=0;i<4;++i){const float t=start+(end-start)*float(i)/3.f;const Point<float> u(std::sin(t),-std::cos(t)); // a mark at each detent
    g.setColour(std::abs(value*3.f-float(i))<.02f?ink:dim);g.drawLine({centre+u*(radius+3),centre+u*(radius+8)},1.6f);}
   for(int i=0;i<4;++i){float a=start+(end-start)*float(i)/3.f;auto p=centre+Point<float>(std::sin(a),-std::cos(a))*(radius+15);
    g.setColour(std::abs(value*3.f-float(i))<.5f?ink:dim);g.drawText(names[i],Rectangle<float>(p.x-14,p.y-6,28,12),Justification::centred);}
  }
  if(slider.getProperties().contains("rangeSelector")){
   const char* names[]={"64'","32'","16'","8'","4'","2'"};g.setFont(face(11.f));
   for(int i=0;i<6;++i){float a=start+(end-start)*float(i)/5.f;auto p=centre+Point<float>(std::sin(a),-std::cos(a))*(radius+14);
    g.setColour(i==roundToInt(value*5)?ink:dim);g.drawText(names[i],Rectangle<float>(p.x-10,p.y-6,20,12),Justification::centred);
   }
  }
  // Orbit's rendered knob (body and shadow), then its index line turned to the value.
  if(knobSprite.isValid()){g.drawImage(knobSprite,area.withSizeKeepingCentre(diameter*1.6f,diameter*1.6f));
   const auto& line=indexFor(slider.findColour(Slider::rotarySliderFillColourId));const float scale=diameter*1.6f/float(line.getWidth());
   g.drawImageTransformed(line,AffineTransform::translation(-float(line.getWidth())*.5f,-float(line.getHeight())*.5f).scaled(scale).rotated(angle).translated(centre.x,centre.y));return;}
  g.setColour(Colour(0xff111213));g.fillEllipse(area);
  const auto a1=centre+Point<float>(std::sin(angle),-std::cos(angle))*(radius*.35f),a2=centre+Point<float>(std::sin(angle),-std::cos(angle))*(radius*.85f);
  g.setColour(ink);g.drawLine({a1,a2},1.8f);
 }
 void drawComboBox(Graphics& g,int w,int h,bool,int,int,int,int,ComboBox&) override {Path arrow;arrow.addTriangle(float(w)-17,float(h)*.5f-4,float(w)-7,float(h)*.5f-4,float(w)-12,float(h)*.5f+3);g.setColour(orbitui::led);g.fillPath(arrow);}
 void positionComboBoxText(ComboBox& box,Label& label) override {label.setBounds(5,0,box.getWidth()-26,box.getHeight());label.setFont(getComboBoxFont(box));}
};

class PianoKeyboard : public MidiKeyboardComponent {
public:
 using MidiKeyboardComponent::MidiKeyboardComponent;
 const OrbitSkin* skin = nullptr;
 bool skinned() const { return skin != nullptr && skin->ready; }
 // With the rendered keyboard the keys are in the background picture: only a held or hovered key is tinted.
 void tint(Graphics& g, int note, Rectangle<float> r, bool down, bool over, bool black) {
  if (!down && !over) return;
  Graphics::ScopedSaveState state(g);
  if (!black) for (int neighbour : {note - 1, note + 1}) if (MidiMessage::isMidiNoteBlack(neighbour)) g.excludeClipRegion(getRectangleForKey(neighbour).getSmallestIntegerContainer()); // not over the black keys
  g.setColour(down ? orbitui::amber.withAlpha(black ? .55f : .42f) : Colours::white.withAlpha(black ? .10f : .16f));
  g.fillRoundedRectangle(r.reduced(1.f, 0.f), 2.f);
 }
 void drawWhiteNote(int note, Graphics& g, Rectangle<float> r, bool down, bool over, Colour, Colour) override {
  if (skinned()) { tint(g, note, r, down, over, false); return; }
  auto key = r.reduced(.6f, 0);
  const auto ivory = down ? Colour(0xffe4bb7c) : Colour(0xffe8e1d0);
  ColourGradient material(Colour(0xff9b968a), key.getX(), key.getY(), ivory, key.getCentreX(), key.getBottom(), false);
  material.addColour(.15, ivory.brighter(.08f)); material.addColour(.92, ivory);
  material.addColour(.96, ivory.darker(.15f));
  g.setGradientFill(material); g.fillRoundedRectangle(key, 2);
  g.setColour(Colours::black.withAlpha(.65f)); g.drawRoundedRectangle(key, 2, .6f);
  g.setColour(Colours::white.withAlpha(.3f)); g.drawVerticalLine(int(key.getX()) + 1, key.getY() + 12, key.getBottom() - 8);
  if (over && !down) { g.setColour(Colours::white.withAlpha(.12f)); g.fillRoundedRectangle(key, 2); }

 }
 void drawBlackNote(int note, Graphics& g, Rectangle<float> r, bool down, bool over, Colour) override {
  if (skinned()) { tint(g, note, r, down, over, true); return; }
  for (int i = 4; i > 0; --i) { g.setColour(Colours::black.withAlpha(.09f)); g.fillRoundedRectangle(r.translated(2, 4).expanded(float(i)), 2); }
  g.setColour(Colour(0xff090a0a)); g.fillRoundedRectangle(r, 2);
  auto cap = r.reduced(2, 1).withTrimmedBottom(down ? 3.f : 7.f);
  ColourGradient material(Colour(0xff3e4140), cap.getX(), cap.getY(), Colour(0xff171919), cap.getRight(), cap.getY(), false);
  material.addColour(.2, Colour(0xff292d2c)); material.addColour(.85, Colour(0xff161818));
  g.setGradientFill(material); g.fillRoundedRectangle(cap, 2);
  g.setColour(Colours::white.withAlpha(.15f)); g.drawVerticalLine(int(cap.getX()) + 1, cap.getY() + 3, cap.getBottom() - 3);
  if (down || over) { g.setColour(down ? orbitui::amber.withAlpha(.4f) : Colours::white.withAlpha(.1f)); g.fillRoundedRectangle(cap, 2); }

 }
};

// Row or column of latching buttons with LEDs, bound to one choice or bool parameter.
class ChoiceButtons : public Component {
public:
 ChoiceButtons(RangedAudioParameter& parameter, const StringArray& names, bool vertical, Colour colour)
  : column(vertical), attachment(parameter, [this](float value) { for (int i = 0; i < buttons.size(); ++i) buttons[i]->setToggleState(i == roundToInt(value), dontSendNotification); }, nullptr) {
  for (int i = 0; i < names.size(); ++i) {
   auto* button = buttons.add(new TextButton(names[i]));
   button->getProperties().set("led", 0); orbitui::tint(*button, colour); button->setWantsKeyboardFocus(false);
   button->onClick = [this, i] { attachment.setValueAsCompleteGesture(float(i)); };
   addAndMakeVisible(button);
  }
  attachment.sendInitialUpdate();
 }
 void resized() override {
  const int count = buttons.size(), gap = 5; if (count == 0) return;
  if (column) { const int each = jmin(30, (getHeight() - gap * (count - 1)) / count); for (int i = 0; i < count; ++i) buttons[i]->setBounds(0, i * (each + gap), getWidth(), each); }
  else { const int each = (getWidth() - gap * (count - 1)) / count, top = ledOnTop ? 0 : 12; for (int i = 0; i < count; ++i) buttons[i]->setBounds(i * (each + gap), top, each, jmax(20, getHeight() - top)); }
 }
 // Plain coloured caps with an LED above each key and the legend printed on the panel above it,
 // instead of the dark caps with a light bar.
 void useColouredCaps(Colour colour) {
  ledOnTop = true;
  for (auto* button : buttons) { button->getProperties().set("led", 1); button->getProperties().set("cream", true); button->getProperties().set("legendAbove", true); orbitui::tint(*button, colour); }
  resized();
 }
private:
 bool column, ledOnTop = false; OwnedArray<TextButton> buttons; ParameterAttachment attachment;
};

// Ivory hardware keys with printed legends, bound to the original parameters.
class PanelChoice : public Component {
public:
 PanelChoice(RangedAudioParameter& parameter,const StringArray& names,bool vertical=false):column(vertical),attachment(parameter,[this](float value){for(int i=0;i<buttons.size();++i)buttons[i]->setToggleState(i==roundToInt(value),dontSendNotification);},nullptr){
  setName(parameter.getName(60));
  for(int i=0;i<names.size();++i){auto* button=buttons.add(new TextButton(column?String():names[i]));button->setTitle(parameter.getName(60)+" "+names[i]);button->setWantsKeyboardFocus(false);
   if(!column)button->getProperties().set("legendAbove",true);else button->getProperties().set("led",2);
   button->onClick=[this,i]{attachment.setValueAsCompleteGesture(float(i));};addAndMakeVisible(button);
   if(column){auto* label=labels.add(new Label({},names[i]));label->setFont(orbitui::face(12.f));label->setColour(Label::textColourId,orbitui::ink);label->setBorderSize({});label->setInterceptsMouseClicks(false,false);addAndMakeVisible(label);}
  }attachment.sendInitialUpdate();
 }
 void resized() override{const int n=buttons.size();for(int i=0;i<n;++i){if(column){buttons[i]->setBounds(0,i*25,34,24);labels[i]->setBounds(37,i*25, getWidth()-37,24);}else{int step=getWidth()/n;buttons[i]->setBounds(i*step,0,step-1,getHeight());}}}
private:bool column;OwnedArray<TextButton> buttons;OwnedArray<Label> labels;ParameterAttachment attachment;
};

// Two-position slide switch bound to a choice parameter: a short fader with one stop at each end and the two
// names printed beside the stops. The first choice is the upper position. Click a name or drag the cap.
class SlideSwitch : public Component {
public:
 SlideSwitch(RangedAudioParameter& parameter, const StringArray& names)
  : last(jmax(1, names.size() - 1)), attachment(parameter, [this](float value) { slider.setValue(double(last - roundToInt(value)), dontSendNotification); }, nullptr) {
  slider.setSliderStyle(Slider::LinearVertical); slider.setTextBoxStyle(Slider::NoTextBox, false, 0, 0); slider.setRange(0.0, double(last), 1.0);
  slider.getProperties().set("marks", last); slider.setWantsKeyboardFocus(false); slider.setName(parameter.getName(60));
  slider.onValueChange = [this] { attachment.setValueAsCompleteGesture(float(last - roundToInt(slider.getValue()))); };
  addAndMakeVisible(slider);
  for (int i = 0; i <= last; ++i) {
   auto* text = legends.add(new Label({}, names[i]));
   text->setFont(orbitui::face(12.f)); text->setColour(Label::textColourId, orbitui::ink);
   text->setJustificationType(Justification::centredLeft); text->setInterceptsMouseClicks(false, false); text->setBorderSize({});
   addAndMakeVisible(text);
  }
  attachment.sendInitialUpdate();
 }
 void resized() override { // the cap travels from 12 px below the top to 12 px above the bottom of the slider
  slider.setBounds(jmax(0, getWidth() / 2 - 38), 0, 28, getHeight());
  const int x = slider.getRight() + 8, width = getWidth() - x;
  for (int i = 0; i <= last; ++i) legends[i]->setBounds(x, 4 + (getHeight() - 24) * i / last, width, 16); // one name level with each stop
 }
 void mouseDown(const MouseEvent& event) override { if (event.x > slider.getRight()) attachment.setValueAsCompleteGesture(float(jlimit(0, last, roundToInt(float(event.y - 12) * float(last) / float(getHeight() - 24))))); }
 void setLever() { slider.getProperties().set("lever", true); for (auto* text : legends) text->setFont(orbitui::face(11.f)); }
 void hideLegends() { for (auto* text : legends) text->setVisible(false); } // printed on the rendered panel instead
private:
 int last; Slider slider; OwnedArray<Label> legends; ParameterAttachment attachment;
};

// Stepped rotary selector with a waveform symbol (or a short text) at each position, bound to a choice parameter.
// Turn the knob or click a symbol; the selected symbol lights up and its name shows underneath.
class WaveKnob : public Component {
public:
 WaveKnob(AudioProcessorValueTreeState& state, const char* id, const StringArray& shapes, bool nameBelow = true) : names(shapes), showName(nameBelow) {
  knob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag); knob.setTextBoxStyle(Slider::NoTextBox, false, 0, 0);
  knob.setRotaryParameters(startAngle, endAngle, true); knob.setMouseDragSensitivity(90);
  knob.getProperties().set("selector", true); knob.setWantsKeyboardFocus(false); knob.setName(state.getParameter(id)->getName(60));
  addAndMakeVisible(knob);
  attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(state, id, knob);
  knob.onValueChange = [this] { repaint(); };
 }
 void resized() override { knob.setBounds(getWidth() / 2 - 25, centreY - 25, 50, 50); }
 void setKnobColour(Colour colour) { knob.setColour(Slider::rotarySliderFillColourId, colour); }
 void paint(Graphics& g) override {
  const int current = jlimit(0, names.size() - 1, roundToInt(knob.getValue()));
  for (int i = 0; i < names.size(); ++i) { const bool on = i == current; g.setColour(on ? orbitui::led : orbitui::dim); drawShape(g, names[i], Rectangle<float>(14, 9).withCentre(symbolCentre(i)), on ? 1.5f : 1.f); }
  ignoreUnused(showName);
 }
 void mouseDown(const MouseEvent& event) override { for (int i = 0; i < names.size(); ++i) if (symbolCentre(i).getDistanceFrom(event.position) < 14) knob.setValue(i, sendNotificationSync); }
private:
 static constexpr float startAngle = MathConstants<float>::pi * 1.3f, endAngle = MathConstants<float>::pi * 2.7f;
 enum { centreY = 42 };
 // Each symbol sits around the knob, at the angle the index line points to for that position.
 Point<float> symbolCentre(int index) const {
  const float angle = startAngle + (endAngle - startAngle) * float(index) / float(jmax(1, names.size() - 1)), radius = 33.f;
  return {float(getWidth()) * .5f + std::sin(angle) * radius, float(centreY) - std::cos(angle) * radius};
 }
 static void drawShape(Graphics& g, const String& name, Rectangle<float> r, float thickness) {
  Path path; const float x = r.getX(), mid = r.getCentreX(), right = r.getRight(), top = r.getY(), bottom = r.getBottom();
  if (name == "MIX") { // saw and pulse drawn over each other: the blended legacy waveform
   path.startNewSubPath(x, bottom); path.lineTo(right, top); path.lineTo(right, bottom);
   path.startNewSubPath(x, bottom); path.lineTo(x, top); path.lineTo(mid, top); path.lineTo(mid, bottom);
  }
  else if (name == "SAW") { path.startNewSubPath(x, bottom); path.lineTo(right, top); path.lineTo(right, bottom); }
  else if (name == "PULSE" || name == "SQUARE") { const float edge=name=="PULSE"?x+r.getWidth()*.25f:mid; path.startNewSubPath(x, bottom); path.lineTo(x, top); path.lineTo(edge, top); path.lineTo(edge, bottom); path.lineTo(right, bottom); }
  else if (name == "TRI") { path.startNewSubPath(x, bottom); path.lineTo(mid, top); path.lineTo(right, bottom); }
  else if (name == "SINE") { for (int k = 0; k <= 16; ++k) { const float px = x + r.getWidth() * float(k) / 16.f, py = r.getCentreY() - std::sin(MathConstants<float>::twoPi * float(k) / 16.f) * r.getHeight() * .5f; if (k == 0) path.startNewSubPath(px, py); else path.lineTo(px, py); } }
  else { g.setFont(orbitui::face(12.f)); g.drawText(name, r.expanded(8, 3), Justification::centred); return; } // octave numbers, routing names
  g.strokePath(path, PathStrokeType(thickness, PathStrokeType::curved, PathStrokeType::rounded));
 }
 StringArray names; bool showName; Slider knob; std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// Small button that shows the current value of a choice parameter and opens a menu to change it.
class ChoiceMenuButton : public TextButton {
public:
 explicit ChoiceMenuButton(RangedAudioParameter& choice)
  : parameter(choice), attachment(choice, [this](float value) { const auto names = parameter.getAllValueStrings(); setButtonText(names[jlimit(0, names.size() - 1, roundToInt(value))]); }, nullptr) {
  getProperties().set("liveText", true); // its legend changes, so it is drawn rather than printed on the rendered cap
  onClick = [this] {
   PopupMenu menu; const auto names = parameter.getAllValueStrings(); const int current = roundToInt(parameter.convertFrom0to1(parameter.getValue()));
   for (int i = 0; i < names.size(); ++i) menu.addItem(i + 1, names[i], true, i == current);
   menu.showMenuAsync(PopupMenu::Options().withTargetComponent(this), [safe = Component::SafePointer<ChoiceMenuButton>(this)](int result) { if (safe != nullptr && result > 0) safe->attachment.setValueAsCompleteGesture(float(result - 1)); });
  };
  attachment.sendInitialUpdate();
 }
private:
 RangedAudioParameter& parameter; ParameterAttachment attachment;
};

// Key with an LED that puts a time control on the tempo: off, the knob sets a free time; on, the same place holds a
// stepped knob over the note values. The key remembers the last note value, so switching back and forth keeps it.
class SyncKey : public TextButton {
public:
 SyncKey(RangedAudioParameter& choice, int fallback) : remembered(fallback),
  attachment(choice, [this](float value) { const int index = roundToInt(value); if (index > 0) remembered = index; setToggleState(index > 0, dontSendNotification); }, nullptr) {
  setButtonText("SYNC"); getProperties().set("led", 2);
  onClick = [this] { attachment.setValueAsCompleteGesture(getToggleState() ? 0.f : float(remembered)); };
  attachment.sendInitialUpdate();
 }
private:
 int remembered; ParameterAttachment attachment;
};

// Stepped knob over the note values of a sync choice; FREE is left to the SYNC key.
class SyncKnob : public Slider {
public:
 explicit SyncKnob(RangedAudioParameter& choice) : names(choice.getAllValueStrings()),
  attachment(choice, [this](float value) { const int index = roundToInt(value); if (index > 0) setValue(index, dontSendNotification); }, nullptr) {
  setSliderStyle(RotaryHorizontalVerticalDrag); setTextBoxStyle(NoTextBox, false, 0, 0); setRange(1, jmax(2, names.size() - 1), 1);
  setMouseDragSensitivity(120);
  onDragStart = [this] { attachment.beginGesture(); }; onDragEnd = [this] { attachment.endGesture(); };
  onValueChange = [this] { attachment.setValueAsPartOfGesture(float(getValue())); };
  getProperties().set("overlay", true); // sits over the free knob: not part of the rendered layout
  attachment.sendInitialUpdate();
 }
 String getTextFromValue(double value) override { return names[jlimit(0, names.size() - 1, roundToInt(value))]; }
private:
 StringArray names; ParameterAttachment attachment;
};

class Editor : public AudioProcessorEditor, private Timer {
 using APVTS = AudioProcessorValueTreeState;
 enum { designW = 1600, designH = 1000, left = 32, contentW = 1536, gap = 12 };
 enum { cheek = 26, frameW = designW + 2 * cheek }; // wide wooden end cheeks around the design surface
 enum { outputW = 116, keysW = 1536 - 186 - outputW - 8 };Button* chordKey=nullptr;TextButton tapButton{"TAP"};double lastTap=0;Array<double> tapGaps; // the keyboard leaves room for OUTPUT on its right
 enum { headerY = 4, lineY = 104, panelY = 120, panelH = 242, row2Y = 382, row2H = 224, lowerY = 624, lowerH = 120, keysY = 764, keysH = 218 }; // Space below effects separates the keyboard from the editing surface.
 struct Section { String title; Colour colour; Rectangle<int> area; int page; };
 struct Fader { Slider slider; String id; bool followsLink = false; std::unique_ptr<APVTS::SliderAttachment> attachment; };
 struct Toggle { TextButton button; std::unique_ptr<APVTS::ButtonAttachment> attachment; };
 struct Surface : Component { explicit Surface(Editor& e) : owner(e) {} void paint(Graphics& g) override { owner.paintSurface(g); } Editor& owner; };
 // Hands out consecutive sections of one row; widths are given in fader-sized units.
 struct Row {
  Editor& editor; int x, y, height, page; float unit;
  Row(Editor& e, int rowY, int rowH, int rowPage, float totalUnits, int count, int origin = left, int availableWidth = contentW) : editor(e), x(origin), y(rowY), height(rowH), page(rowPage), unit(float(availableWidth - gap * (count - 1)) / totalUnits) {}
  Rectangle<int> next(const String& title, Colour colour, float units) {
   const int width = roundToInt(units * unit); const Rectangle<int> area(x, y, width, height);
   editor.sections.push_back({title, colour, area, page}); editor.sectionColour = colour; x += width + gap; return area.withTrimmedTop(34);
  }
 };
public:
 explicit Editor(Orbit& p) : AudioProcessorEditor(p), orbit(p), surface(*this), keyboard(p.keyboard, MidiKeyboardComponent::horizontalKeyboard) {
  setLookAndFeel(&look);
  addAndMakeVisible(surface); surface.setBounds(0, 0, designW, designH);
  buildHeader(); buildPanelA(); buildRowTwo(); buildLowerRow(); buildKeyboard();
  setPage(0);
  // This design is self-contained: legacy external skins cannot override the approved layout.
  setResizable(true, true); getConstrainer()->setFixedAspectRatio(double(frameW) / double(designH));
  setResizeLimits(frameW * 8 / 10, designH * 8 / 10, frameW * 3 / 2, designH * 3 / 2);
  setSize(frameW * 9 / 10, designH * 9 / 10);
  timerCallback(); startTimerHz(12);
  forceMonitor = orbitforce::start([safe = Component::SafePointer<Editor>(this)](float pressure, int stage) { if (safe != nullptr) safe->trackpadPressure(pressure, stage); });
 }
 // Force Touch trackpad: pressing harder than a click on the panel keyboard sends key pressure (a deep click is full
 // pressure), so PRESS and the other pressure knobs can be tried without a MIDI keyboard.
 void trackpadPressure(float pressure, int stage) {
  const bool playing = keyboard.isMouseButtonDown();
  const float amount = !playing || stage < 1 ? 0.f : stage >= 2 ? 1.f : jlimit(0.f, 1.f, (pressure - .1f) / .9f);
  const int value = roundToInt(amount * 127.f);
  if (value != sentPressure) { sentPressure = value; orbit.screenPressure.store(value); }
 }
 ~Editor() override { if(pitchWheel.getValue()!=0)orbit.screenPitch.store(8192); orbitforce::stop(forceMonitor); stopTimer(); setLookAndFeel(nullptr); }
 void paint(Graphics& g) override { // the end cheeks: one solid piece of wood each side, the panel sits between them
  g.fillAll(Colours::black); g.addTransform(AffineTransform::scale(float(getWidth()) / float(frameW)));
  const int w = cheek + 16;
  for (int x : {0, frameW - w}) {
   const bool right = x > 0; const float a = float(x), b = float(x + w);
   ColourGradient wood(Colour(0xff1c120c), right ? b : a, 0, Colour(0xff1c120c), right ? a : b, 0, false);
   wood.addColour(.30, Colour(0xff5e432c)); wood.addColour(.55, Colour(0xff4a3322)); wood.addColour(.85, Colour(0xff2e1f15));
   g.setGradientFill(wood); g.fillRect(x, 0, w, designH);
   Random grain(right ? 7 : 3); // fixed seed: the same grain on every repaint
   for (int i = 1; i < w; i += 2) { g.setColour(Colours::black.withAlpha(.08f + grain.nextFloat() * .18f)); g.drawVerticalLine(x + i, 0, float(designH)); }
   g.setColour(Colour(0xff8a6644).withAlpha(.35f)); g.drawVerticalLine(right ? x + w - 2 : x + 1, 0, float(designH)); // rounded outer edge catching light
   g.setColour(Colours::black.withAlpha(.6f)); g.fillRect(right ? x : x + w - 2, 0, 2, designH); // shadow where the panel meets the wood
  }
 }
 void resized() override { const float s = float(getWidth()) / float(frameW); surface.setTransform(AffineTransform::scale(s).translated(float(cheek) * s, 0)); }

private:
 // ---- construction helpers -------------------------------------------------------------
 static std::vector<Rectangle<int>> split(Rectangle<int> area, std::initializer_list<float> weights) {
  float total = 0; for (float w : weights) total += w;
  std::vector<Rectangle<int>> cells; float x = float(area.getX());
  for (float w : weights) { const float width = float(area.getWidth()) * w / total; cells.emplace_back(roundToInt(x), area.getY(), roundToInt(width), area.getHeight()); x += width; }
  return cells;
 }
 void place(Component& component, Rectangle<int> bounds, int page) {
  if (&component != &keyboard) component.setWantsKeyboardFocus(false); // typing keys always go to the piano keyboard
  surface.addAndMakeVisible(component); component.setBounds(bounds); if (page >= 0) paged.push_back({&component, page}); }
 void caption(const String& text, Rectangle<int> bounds, int page, float size = 15.f, Colour colour = orbitui::ink) {
  auto* label = labels.add(new Label({}, text));
  label->setFont(orbitui::legend(jmax(13.f, size))); label->setColour(Label::textColourId, colour); label->setJustificationType(Justification::centred);
  label->setBorderSize({}); label->setMinimumHorizontalScale(.9f); label->setInterceptsMouseClicks(false, false);
  place(*label, bounds, page);
 }
 void setUpSlider(Slider& slider, const char* id, Slider::SliderStyle style) {
  auto* parameter = orbit.state.getParameter(id);
  slider.setSliderStyle(style); slider.setTextBoxStyle(Slider::NoTextBox, false, 0, 0);
  slider.setPopupDisplayEnabled(true, true, &surface); slider.setName(parameter->getName(60));
 }
 Fader& addControl(const char* id, Slider::SliderStyle style, Rectangle<int> bounds, int page) {
  auto* fader = faders.add(new Fader()); fader->id = id;fader->slider.getProperties().set("parameterId",id);
  setUpSlider(fader->slider, id, style); fader->slider.setColour(Slider::rotarySliderFillColourId, sectionColour); place(fader->slider, bounds, page);
  fader->attachment = std::make_unique<APVTS::SliderAttachment>(orbit.state, id, fader->slider);
  auto* parameter = orbit.state.getParameter(id);
  fader->slider.setDoubleClickReturnValue(true, parameter->convertFrom0to1(parameter->getDefaultValue()));
  return *fader;
 }
 Fader& addFader(const char* id, const char* text, int page, Rectangle<int> cell) {
  caption(text, cell.withHeight(16), page);
  return addControl(id, Slider::LinearVertical, Rectangle<int>(cell.getCentreX() - 20, cell.getY() + 28, 40, cell.getHeight() - 38), page);
 }
 void addEffectScale(const char* id, Rectangle<int> cell) {
  const auto range = orbit.state.getParameter(id)->getNormalisableRange();
  ignoreUnused(range, id); // every knob reads 0 to 10: just the two ends of the scale, at the foot of the knob
  const int cx = cell.getCentreX(), y = cell.getY() + 67;
  caption("0", {cx - 25, y - 5, 12, 10}, -1, 8.f, orbitui::dim); caption("10", {cx + 13, y - 5, 12, 10}, -1, 8.f, orbitui::dim); // kept inside the knob's own column
 }
 void addKnob(const char* id, const char* text, Rectangle<int> cell, bool showScale = false) {
  caption(text, cell.withHeight(16), -1, 13.f);
  const int width = jmin(60, cell.getWidth());
  auto& control = addControl(id, Slider::RotaryHorizontalVerticalDrag,
    {cell.getCentreX() - width / 2, cell.getY() + 15, width, jmin(54, cell.getHeight() - 18)}, -1);
  if (showScale) addEffectScale(id, cell);
  if (StringRef(id) == StringRef("gain")) control.slider.getProperties().set("faceDiameter", 44.f);
 }
 void addRowKnob(const char* id, const char* text, Rectangle<int> cell) {
  caption(text, cell.withHeight(16), 0);
  const int width = jmin(56, cell.getWidth());
  auto& control = addControl(id, Slider::RotaryHorizontalVerticalDrag, Rectangle<int>(cell.getCentreX() - width / 2, cell.getY() + 26, width, 56), 0);
  control.slider.getProperties().set("faceDiameter", 39.f);
  auto* valueLabel = labels.add(new Label());
  valueLabel->setFont(orbitui::legend(13.f)); valueLabel->setColour(Label::textColourId, orbitui::ink);
  valueLabel->setJustificationType(Justification::centred); valueLabel->setInterceptsMouseClicks(false, false);
  valueLabel->getProperties().set("live", true); // changes with the control: never printed in the rendered artwork
  place(*valueLabel, {cell.getX(), cell.getY() + 77, cell.getWidth(), 14}, 0);
  auto update = [valueLabel, slider = &control.slider] { auto text = slider->getTextFromValue(slider->getValue());
   if (text.containsChar('(')) text = text.fromFirstOccurrenceOf("(", false, false).upToFirstOccurrenceOf(")", false, false);
   valueLabel->setText(text, dontSendNotification); };
  control.slider.onValueChange = update; update();
 }
 void addChoice(const char* id, const char* text, const StringArray& names, int page, Rectangle<int> cell, bool vertical, Colour colour = orbitui::neutral) {
  if (vertical && names.size() <= 3) { // a choice between two or three: a slide switch under its caption
   caption(text, cell.withHeight(16), page);
   place(*switches.add(new SlideSwitch(*orbit.state.getParameter(id), names)), {cell.getX(), cell.getY() + 22, cell.getWidth(), 52}, page);
   return;
  }
  auto* choice = choices.add(new ChoiceButtons(*orbit.state.getParameter(id), names, vertical, colour));
  if (vertical) { caption(text, cell.withHeight(16), page); place(*choice, Rectangle<int>(cell.getX() + 3, cell.getY() + 28, cell.getWidth() - 6, jmin(names.size() * 35, cell.getHeight() - 30)), page); }
  else { place(*choice, cell.withHeight(46).reduced(3, 0), page); caption(text, Rectangle<int>(cell.getX(), cell.getY() + 52, cell.getWidth(), 14), page, 11.f, orbitui::dim); }
 }
 void addWaveKnob(const char* id, const char* text, const StringArray& names, int page, Rectangle<int> cell, bool nameBelow = true) {
  caption(text, cell.withHeight(16).translated(0, -5), page); // raised a little: a symbol sits right above the knob
  auto* selector = waveKnobs.add(new WaveKnob(orbit.state, id, names, nameBelow)); selector->setKnobColour(sectionColour);
  place(*selector, {cell.getX(), cell.getY() + 12, cell.getWidth(), 80}, page); // knob centre level with the plain knobs of the row
 }
 // A small knob with its caption, for columns of the second row that stack two controls.
 void addMiniKnob(const char* id, const char* text, Rectangle<int> area) {
  caption(text, area.withHeight(14), 0, 12.f);
  const int width = jmin(50, area.getWidth());
  addControl(id, Slider::RotaryHorizontalVerticalDrag, Rectangle<int>(area.getCentreX() - width / 2, area.getY() + 14, width, 48), 0);
 }
 void addToggle(const char* id, const String& text, int page, Rectangle<int> bounds, int led, Colour colour) {
  auto* toggle = toggles.add(new Toggle());toggle->button.getProperties().set("parameterId",id);
  toggle->button.setButtonText(text); toggle->button.setClickingTogglesState(true); toggle->button.getProperties().set("led", led); orbitui::tint(toggle->button, colour);
  place(toggle->button, bounds, page);
  toggle->attachment = std::make_unique<APVTS::ButtonAttachment>(orbit.state, id, toggle->button);
 }
 // SYNC key at keyBounds, and a stepped note-value knob stacked over the free knob; only one of the two knobs shows.
 void addSync(const char* id, int fallback, Slider& freeKnob, Rectangle<int> keyBounds, int page, Label* value) {
  auto* sync = syncs.add(new Sync()); sync->id = id; sync->free = &freeKnob; sync->value = value;
  auto& parameter = *orbit.state.getParameter(id);
  sync->key = std::make_unique<SyncKey>(parameter, fallback); sync->key->getProperties().set("parameterId",id);sync->key->getProperties().set("legendAbove",true); orbitui::tint(*sync->key, orbitui::neutral); place(*sync->key, keyBounds, page);
  sync->knob = std::make_unique<SyncKnob>(parameter); sync->knob->setPopupDisplayEnabled(true, true, &surface); sync->knob->setName(parameter.getName(60));
  sync->knob->setColour(Slider::rotarySliderFillColourId, freeKnob.findColour(Slider::rotarySliderFillColourId));
  if (freeKnob.getProperties().contains("faceDiameter")) sync->knob->getProperties().set("faceDiameter", freeKnob.getProperties()["faceDiameter"]);
  place(*sync->knob, freeKnob.getBounds(), page);
 }
 void chipChoice(const char* id,const char* title,const StringArray& names,Rectangle<int> cell,bool vertical=false){
  caption(title,cell.withHeight(15),-1,12.f);
  if(vertical&&names.size()<=3){ // a two- or three-way choice is a slide switch, its names beside the stops
   auto* control=switches.add(new SlideSwitch(*orbit.state.getParameter(id),names));control->getProperties().set("parameterId",id);
   place(*control,cell.withTrimmedTop(18),-1);return;}
  auto* control=panelChoices.add(new PanelChoice(*orbit.state.getParameter(id),names,vertical));control->getProperties().set("parameterId",id);
  place(*control,cell.withTrimmedTop(18),-1);
 }
 void compactKnob(const char* id,const char* title,Rectangle<int> cell){
  caption(title,cell.withHeight(14),-1,11.f);
  auto& control=addControl(id,Slider::RotaryHorizontalVerticalDrag,{cell.getX(),cell.getY()+17,cell.getWidth(),42},-1);
  control.slider.getProperties().set("faceDiameter",31.f);
 }
 void menuChoice(const char* id,const char* title,Rectangle<int> cell){
  caption(title,cell.withHeight(14),-1,11.f);
  auto* button=menuButtons.add(new ChoiceMenuButton(*orbit.state.getParameter(id)));button->getProperties().set("parameterId",id);
  place(*button,{cell.getX(),cell.getY()+19,cell.getWidth(),28},-1);
 }
 void printedToggle(const char* id,const char* title,Rectangle<int> cell){
  addToggle(id,title,-1,cell.withHeight(45),0,orbitui::neutral);toggles.getLast()->button.getProperties().set("legendAbove",true);
 }
 Rectangle<int> section(const char* title,Colour colour,Rectangle<int> area){sections.push_back({title,colour,area,-1});sectionColour=colour;return area.withTrimmedTop(36).reduced(8,0);}
 void useOwnFilterEnvelope() {
  auto* link = orbit.state.getParameter("envLink");
  if (link->getValue() > .5f) { link->beginChangeGesture(); link->setValueNotifyingHost(0.f); link->endChangeGesture(); }
 }
 void addButton(TextButton& button, const String& text, Rectangle<int> bounds, Colour colour, int led) {
  button.setButtonText(text); button.getProperties().set("led", led); button.getProperties().set("cream", true); // memory and utility keys keep light caps
  orbitui::tint(button, colour); place(button, bounds, -1);
 }

 // ---- header: name, LED display, bank / patch memory, save / load, panel switch --------
 void buildHeader() {
  using namespace orbitui;
  refreshPresetMenu();
  presets.setSelectedId(orbit.getCurrentProgram() + 1, dontSendNotification); place(presets, {518, 53, 330, 32}, -1);
  presets.onChange = [this] {
   const int id = presets.getSelectedId();
   if (id >= 2000) { const int index = id - 2000; if (isPositiveAndBelow(index, soundNames.size())) showResult(orbit.loadSound(soundNames[index])); }
   else if (id >= 1000) { const int index = id - 1000; if (isPositiveAndBelow(index, userNames.size())) showResult(orbit.loadUserPreset(userNames[index])); }
   else if (id > 0) orbit.setCurrentProgram(id - 1);
  };
  for (int i = 0; i < 3; ++i) { auto* button = bankButtons.add(new TextButton()); addButton(*button, String::charToString(juce_wchar('A' + i)), {876 + i * 41, 39, 36, 45}, neutral, 1); button->onClick = [this, i] { shownBank = i; timerCallback(); }; }
  for (int i = 0; i < 8; ++i) { auto* button = patchButtons.add(new TextButton()); addButton(*button, String(i + 1), {1024 + i * 39, 39, 34, 45}, i < 4 ? bright : neutral, 1); button->onClick = [this, i] { orbit.setCurrentProgram(shownBank * 8 + i); timerCallback(); }; }
  addButton(saveButton, "SAVE", {1416, 39, 70, 44}, amber, 1); saveButton.onClick = [this] { saveDialog(); };
  addButton(midiButton, "MIDI", {1344, 39, 60, 44}, neutral, 1); midiButton.onClick = [this] { midiMenu(); }; // MIDI learn
  addButton(loadButton, "LOAD", {1494, 39, 70, 44}, neutral, 1);
  loadButton.onClick = [this] { refreshPresetMenu(); if (orbit.soundName().isNotEmpty()) showResult(orbit.loadSound(orbit.soundName())); else if (orbit.userPresetName().isNotEmpty()) showResult(orbit.loadUserPreset(orbit.userPresetName())); else orbit.setCurrentProgram(orbit.getCurrentProgram()); };
 }

// ---- first row: the playing faders ------------------------------------------------------
 void buildPanelA(){
  using namespace orbitui;
  // POLY-MOD: two signed sources (filter envelope, VCO 1), three destinations switched on and off.
  // POLY-MOD: two signed sources (filter envelope, VCO 1), six destinations switched on and off: VCO 2 pitch, pulse
  // width and shape, the filters' cutoff, the filter MODE (MULTI) and a blend into band-pass.
  auto area=section("POLY-MOD",steel,{32,panelY,282,panelH});auto cells=split(area,{1,1,.9f,.9f});
  addFader("polyEnv","FILT ENV",0,cells[0].withTrimmedBottom(10));addFader("polyOsc","VCO 1",0,cells[1].withTrimmedBottom(10));
  caption("DESTINATIONS",cells[2].getUnion(cells[3]).withHeight(16),0);
  const char* polyIds[]={"polyFreq","polyWidth","polyShape","polyFilter","polyMode","polyBP"};const char* polyNames[]={"FREQ 2","PW 2","SHAPE 2","FILTER","MODE","BP"};
  for(int i=0;i<6;++i){const auto& column=cells[2+i/3];printedToggle(polyIds[i],polyNames[i],column.withSizeKeepingCentre(52,45).withY(column.getY()+22+(i%3)*58));}
  area=section("MIXER",steel,{320,panelY,238,panelH});cells=split(area,{1,1,1,1.2f});
  addFader("oscLayer","VCO 2",0,cells[0].withTrimmedBottom(26));caption("VCO 1",cells[0].withY(area.getBottom()-23).withHeight(17),0,12);
  addFader("vcoSub","SUB",0,cells[1].withTrimmedBottom(10));addFader("vcoNoise","NOISE",0,cells[2].withTrimmedBottom(10));
  addRowKnob("vcoPwm","VCO PWM",cells[3]);addRowKnob("voiceDrive","DRIVE",cells[3].withTrimmedTop(100)); // the voices' drive, before the filters
  area=section("FILTER A",steel,{564,panelY,432,panelH});cells=split(area,{1,1,1,1,1,1,1,1.65f,1.35f});
  const char* filterIds[]={"hpfFreq","cutoff","resonance","env","lfoFilter","keytrack","velFilter"};const char* filterNames[]={"HPF","FREQ","RES","ENV","LFO","KYBD","VEL"};
  for(int i=0;i<7;++i)addFader(filterIds[i],filterNames[i],0,cells[i].withTrimmedBottom(10));
  chipChoice("filterAType","MODEL",{"LADDER","SVF","MULTI"},cells[7].withHeight(95),true);
  chipChoice("slope","SLOPE",{"12","24"},cells[7].withTrimmedTop(110).withHeight(75),true);
  // MULTI: MODE sweeps low-pass, notch, high-pass; BP takes the band-pass instead.
  addRowKnob("filterMode","LP N HP",cells[8]);printedToggle("filterBP","BP",cells[8].withTrimmedTop(118).withSizeKeepingCentre(44,45));
  area=section("ENV / AMP",steel,{1002,panelY,244,panelH});cells=split(area,{1,1,1,1,1});
  const char* ampIds[]={"attack","decay","sustain","release","velocity"};const char* ampNames[]={"A","D","S","R","VEL"};
  for(int i=0;i<5;++i)addFader(ampIds[i],ampNames[i],0,cells[i].withTrimmedBottom(10));
  area=section("ENV / FILTER",steel,{1252,panelY,316,panelH});cells=split(area,{1,1,1,1,1.9f});
  const char* envIds[]={"fattack","fdecay","fsustain","frelease"};const char* envNames[]={"A","D","S","R"};
  for(int i=0;i<4;++i){auto& fader=addFader(envIds[i],envNames[i],0,cells[i].withTrimmedBottom(10));fader.followsLink=true;fader.slider.onDragStart=[this]{useOwnFilterEnvelope();};}
  chipChoice("envLink","SOURCE",{"OWN","AMP"},cells[4].withHeight(75),true);
  chipChoice("envInvert","POLARITY",{"NORM","INV"},cells[4].withTrimmedTop(100).withHeight(75),true);
 }
 void buildRowTwo(){
  using namespace orbitui;
  auto osc=section("OSCILLATORS",steel,{32,row2Y,470,row2H}); // Vantage: narrower, LFO / MODULATION takes the room
  for(int i=0;i<2;++i){const int y=osc.getY()+i*92;
   caption(i?"VCO 2":"VCO 1",{osc.getX(),y+40,43,22},-1,14);labels.getLast()->setFont(face(17.f,true).withExtraKerningFactor(.04f));labels.getLast()->setMinimumHorizontalScale(.5f); // printed like the rest of the panel
   tagRules.push_back(Rectangle<float>(float(osc.getX()+6),float(y+62),31.f,2.f));                                                 // with a short amber rule under it
   // One continuous SHAPE control per VCO: triangle, saw, square, then a narrowing pulse.
   caption("SHAPE",{osc.getX()+52,y,110,14},-1,12);
   auto& shape=addControl(i?"vco2Wave":"vco1Wave",Slider::RotaryHorizontalVerticalDrag,{osc.getX()+52,y+14,110,74},-1);
   shape.slider.getProperties().set("shapeScale",true);shape.slider.getProperties().set("faceDiameter",32.f);
   shape.slider.setRotaryParameters(MathConstants<float>::pi*1.25f,MathConstants<float>::pi*2.75f,true);
   // Direct choice: a click on TRI, SAW, SQR or PLS puts the knob exactly on that wave (the knob also has a detent on each).
   const Point<float> centre(float(osc.getX()+107),float(y+51));
   for(int k=0;k<4;++k){const float a=MathConstants<float>::pi*(1.25f+1.5f*float(k)/3.f);
    const auto at=centre+Point<float>(std::sin(a),-std::cos(a))*31.f;
    auto* stop=shapeStops.add(new ShapeStop(shape.slider,float(k)/3.f));place(*stop,Rectangle<int>(roundToInt(at.x)-16,roundToInt(at.y)-9,32,18),-1);}

   caption("RANGE",{osc.getX()+170,y,96,14},-1,12);
   auto& range=addControl(i?"osc2Octave":"osc1Octave",Slider::RotaryHorizontalVerticalDrag,{osc.getX()+170,y+14,96,74},-1);
   range.slider.getProperties().set("rangeSelector",true);range.slider.getProperties().set("faceDiameter",32.f);
   range.slider.setRotaryParameters(MathConstants<float>::pi*1.2f,MathConstants<float>::pi*2.8f,true);
   range.slider.setMouseDragSensitivity(140);
   compactKnob(i?"osc2Width":"vco1Width","WIDTH",{osc.getX()+270,y+15,45,68});
   if(!i)compactKnob("crossMod","X-MOD",{osc.getX()+335,y+15,65,68});
   else{compactKnob("osc2Tune","TUNE",{osc.getX()+318,y+15,42,68});compactKnob("detune","FINE",{osc.getX()+360,y+15,42,68});
    printedToggle("oscSync","SYNC",{osc.getX()+405,y+26,27,45});printedToggle("vco2Low","LOW",{osc.getX()+435,y+26,27,45});}
  }
  // LFO: six waves (RAMP: falling saw, S&H: a random step per cycle) and its destinations: PITCH (both VCOs or one),
  // the filter MODE and the level (tremolo), beside the filter, PWM and wheel routings.
  auto mod=section("LFO / MODULATION",teal,{508,row2Y,424,row2H});
  auto top=split(mod.withHeight(86),{1,1,2.7f,1,1,1});
  compactKnob("rate","RATE",top[0]);auto& rateKnob=faders.getLast()->slider;
  compactKnob("lfoDelay","DELAY",top[1]);chipChoice("lfoShape","WAVE",{"TRI","SIN","SAW","RAMP","SQR","S&H"},top[2].withHeight(64));
  compactKnob("lfoPitch","PITCH",top[3]);compactKnob("lfoMode","MODE",top[4]);compactKnob("lfoAmp","AMP",top[5]);
  auto bottom=split(mod.withTrimmedTop(94),{1.8f,1,1.1f,1,1,1});
  auto switches=split(bottom[0].withHeight(57),{1,1,1,1});
  caption({}, {top[0].getX(),top[0].getY()+62,top[0].getWidth(),14},-1,10);auto* rateValue=labels.getLast();rateValue->getProperties().set("live",true);
  addSync("lfoSync",5,rateKnob,switches[0].withHeight(45),-1,rateValue);
  printedToggle("lfoKeyTrig","KEY",switches[1]);printedToggle("envLfoTrig","ENV",switches[2]);printedToggle("lfoManual","MAN",switches[3]);
  menuChoice("pwmSource","PWM",bottom[1]);menuChoice("lfoOsc","P>VCO",bottom[2]);compactKnob("envPitch","ENV>P",bottom[3]);compactKnob("wheelDepth","WHEEL",bottom[4]);compactKnob("wheelFilter","WHL>F",bottom[5]);
  auto filter=section("FILTER B",steel,{938,row2Y,220,row2H});auto cells=split(filter.withHeight(80),{1,1,1,1});
  compactKnob("cutoffB","FREQ",cells[0]);compactKnob("resonanceB","RES",cells[1]);compactKnob("envB","ENV",cells[2]);compactKnob("filterBlend","MIX",cells[3]);
  cells=split(filter.withTrimmedTop(94),{1,1});chipChoice("filterBType","MODEL",{"LADDER","SVF"},cells[0].withHeight(72),true);chipChoice("slopeB","SLOPE",{"12","24"},cells[1].withHeight(72),true);
  auto routing=section("ROUTING",steel,{1164,row2Y,200,row2H});chipChoice("filterRoute","",{"A","B","A>B","B>A","PAR"},routing.withHeight(65));
  caption("PRESSURE",routing.withTrimmedTop(80).withHeight(15),-1,12);
  cells=split(routing.withTrimmedTop(100),{1,1,1,1,1});compactKnob("atFilter","FILT",cells[0]);compactKnob("atMode","MODE",cells[1]);compactKnob("atVolume","AMP",cells[2]);compactKnob("atLfo","LFO",cells[3]);compactKnob("atDrive","DRV",cells[4]);
  // EFFECTS, in three bands: CHORUS (mode and MIX) on a tinted well, PHASER on its own well, then the output DRIVE.
  auto fx=section("EFFECTS",teal,{1370,row2Y,198,row2H});const int quarter=fx.getWidth()*3/4;
  wells.push_back({fx.withHeight(62).expanded(4,0),teal.withAlpha(.22f)});
  chipChoice("chorusMode","CHORUS",{"OFF","I","II","I+II"},fx.withHeight(58).withWidth(quarter));compactKnob("chorus","MIX",fx.withHeight(58).withTrimmedLeft(quarter));
  wells.push_back({fx.withTrimmedTop(65).withHeight(62).expanded(4,0),orbitui::ink.withAlpha(.06f)});
  wells.push_back({fx.withTrimmedTop(130).withHeight(55).expanded(4,0),orbitui::amber.withAlpha(.16f)}); // DRIVE: warm well
  cells=split(fx.withTrimmedTop(67).withHeight(58),{1,.95f,1.15f,1,.9f});
  caption("PHASER",cells[0].withHeight(14),-1,11.f);labels.getLast()->setFont(orbitui::face(14.f)); // condensed, like the switch legends: the band is narrow
  printedToggle("phaserOn","",cells[0].withTrimmedTop(16));
  compactKnob("phaserRate","RATE",cells[1]);compactKnob("phaserDepth","DEPTH",cells[2]);compactKnob("phaserFeedback","FDBK",cells[3]);compactKnob("phaserMix","MIX",cells[4]);
  compactKnob("fxDrive","DRIVE",fx.withTrimmedTop(130).withHeight(58).withSizeKeepingCentre(60,58));
 }
 void buildLowerRow(){
  using namespace orbitui;
  auto area=section("VOICES",teal,{32,lowerY,300,lowerH});auto cells=split(area,{.95f,2.f,.9f,.95f,1.35f,1.55f});
  chipChoice("mode","COUNT",{"6","8"},cells[0].withHeight(65));chipChoice("voiceMode","",{"POLY","MONO","UNI"},cells[1].withHeight(65));
  compactKnob("age","AGE",cells[2]);compactKnob("voiceWidth","WIDTH",cells[3]);menuChoice("voiceSpread","SPREAD",cells[4]); // WIDTH and SPREAD: how wide, and where each voice sits
  chipChoice("vcaMode","VCA",{"ENV","GATE","F.ENV"},cells[5].withHeight(79),true);switches.getLast()->setLever(); // HOLD is beside the keys
  // GLIDE / UNISON: UNI sets how many voices stack in UNI mode, DETUNE how far apart they are tuned.
  area=section("GLIDE / UNISON",teal,{338,lowerY,220,lowerH});cells=split(area,{1.05f,.9f,.85f,1.2f,1});
  compactKnob("portamento","PORTA",cells[0]);compactKnob("bendRange","BEND",cells[1]);menuChoice("unisonVoices","UNI",cells[2]);compactKnob("unisonSpread","DETUNE",cells[3]);printedToggle("legato","LEGATO",cells[4].withTrimmedTop(18));
  area=section("ARPEGGIO",teal,{564,lowerY,280,lowerH});cells=split(area,{.75f,.85f,1.15f,1.f,1.f,1.f,1.f});
  printedToggle("arpOn","ON",cells[0].withTrimmedTop(18));printedToggle("arpSync","SYNC",cells[1].withTrimmedTop(18));menuChoice("arpPattern","MODE",cells[2]);
  menuChoice("arpDivision","STEP",cells[3]);menuChoice("arpOctaves","OCT",cells[4]);compactKnob("arpTempo","TEMPO",cells[5]);compactKnob("arpGate","GATE",cells[6]);
  // TAP: the tempo from the last taps (arpeggio, sequencer, synced delay and LFO when there is no host clock).
  place(tapButton,cells[5].withTrimmedTop(62).withSizeKeepingCentre(42,19),-1);tapButton.getProperties().set("fontSize",11.f);tapButton.setWantsKeyboardFocus(false);
  tapButton.onClick=[this]{const double now=Time::getMillisecondCounterHiRes()*.001,gap=now-lastTap;lastTap=now;
   if(gap<.2||gap>2.){tapGaps.clearQuick();return;}tapGaps.add(gap);if(tapGaps.size()>4)tapGaps.remove(0);
   double sum=0;for(double g:tapGaps)sum+=g;auto* p=orbit.state.getParameter("arpTempo");p->beginChangeGesture();
   p->setValueNotifyingHost(p->convertTo0to1(float(jlimit(40.,300.,60.*tapGaps.size()/sum))));p->endChangeGesture();};
  // SEQUENCER keys: REC, then play one note or chord per step (REST for a silent step); PLAY, then hold a key to transpose.
  // SEQUENCER: its own section, on the arpeggio clock (TEMPO, STEP, GATE, SYNC).
  area=section("SEQUENCER",teal,{850,lowerY,250,lowerH});cells=split(area,{.95f,.95f,.95f,.95f,1.1f,1.5f}); // EDIT and MEMORY apart
  // REC works like a looper pedal: REC (red) records a new sequence; pressed again, OVD (blue): the recording ends, the
  // sequence plays and what you play joins it; pressed again, overdub ends and the sequence keeps playing.
  printedToggle("seqRec","REC",cells[0].withTrimmedTop(18));recKey=&toggles.getLast()->button;toggles.getLast()->attachment.reset();
  recKey->setClickingTogglesState(false);recKey->onClick=[this]{
   auto setParam=[this](const char* id,bool on){auto* p=orbit.state.getParameter(id);p->beginChangeGesture();p->setValueNotifyingHost(on?1.f:0.f);p->endChangeGesture();};
   const bool rec=orbit.state.getRawParameterValue("seqRec")->load()>.5f,ovd=orbit.state.getRawParameterValue("seqOvd")->load()>.5f;
   const bool play=orbit.state.getRawParameterValue("seqPlay")->load()>.5f;
   if(ovd)setParam("seqOvd",false);
   else if(rec){setParam("seqRec",false);setParam("seqPlay",true);setParam("seqOvd",true);}
   else if(play)setParam("seqOvd",true); // REC while the sequence plays: live recording, quantized to the steps
   else{setParam("seqPlay",false);setParam("seqRec",true);}};printedToggle("seqRest","REST",cells[1].withTrimmedTop(18));printedToggle("seqPlay","PLAY",cells[2].withTrimmedTop(18));printedToggle("seqLive","LIVE",cells[3].withTrimmedTop(18));
  restKey=&toggles[toggles.size()-3]->button;restKey->setClickingTogglesState(false); // REST is a momentary key
  // MEMORY: load one of the eight stored sequences, or store the current one, from a menu.
  // EDIT opens the step grid over the panel (the keyboard stays in view for playing the notes in).
  caption("EDIT",cells[4].withHeight(14),-1,11.f);seqEditKey.setClickingTogglesState(true);seqEditKey.getProperties().set("parameterId",String());
  place(seqEditKey,cells[4].withTrimmedTop(26).withSizeKeepingCentre(jmin(cells[4].getWidth()-6,40),26),-1);seqEditKey.onClick=[this]{showSequenceEditor(seqEditKey.getToggleState());};
  surface.addChildComponent(sequenceEditor);sequenceEditor.setBounds(32,row2Y,contentW,lowerY+lowerH-row2Y);sequenceEditor.onClose=[this]{seqEditKey.setToggleState(false,sendNotificationSync);};
  caption("MEMORY",cells[5].withHeight(14),-1,11.f);
  seqMemory.getProperties().set("liveText",true);place(seqMemory,{cells[5].getX(),cells[5].getY()+19,cells[5].getWidth(),28},-1);
  // the step being played or recorded, under MEMORY
  caption({},{cells[5].getX(),cells[5].getY()+52,cells[5].getWidth(),14},-1,10.f,led);seqStepLabel=labels.getLast();seqStepLabel->setFont(mono(11.f));
  seqMemory.onClick=[this]{PopupMenu menu;menu.addSectionHeader("LOAD");
   for(int i=0;i<Orbit::sequenceSlots;++i){const bool used=orbit.sequenceSlotUsed(i);menu.addItem(1+i,"SEQ "+String(i+1)+(used?"   "+String(orbit.sequenceSlotLength(i))+" steps":String("   empty")),used,orbit.sequenceSlot()==i);}
   menu.addSectionHeader("STORE THE CURRENT SEQUENCE");
   for(int i=0;i<Orbit::sequenceSlots;++i)menu.addItem(101+i,"SEQ "+String(i+1)+(orbit.sequenceSlotUsed(i)?"   (replace)":String()),orbit.sequenceLength()>0);
   menu.showMenuAsync(PopupMenu::Options().withTargetComponent(&seqMemory),[safe=Component::SafePointer<Editor>(this)](int result){
    if(safe==nullptr||result<=0)return;if(result>100)safe->orbit.storeSequence(result-101);else safe->orbit.loadSequence(result-1);});};
  restKey->onStateChange=[this]{const bool down=restKey->isDown();if(down==restDown)return;restDown=down;auto* p=orbit.state.getParameter("seqRest");p->beginChangeGesture();p->setValueNotifyingHost(down?1.f:0.f);p->endChangeGesture();};
  area=section("DELAY",teal,{1106,lowerY,210,lowerH});cells=split(area,{1,1,1,1,.75f,.9f});
  compactKnob("delayTime","TIME",cells[0]);delayTimeKnob=&faders.getLast()->slider;
  compactKnob("delayFeedback","FDBK",cells[1]);compactKnob("delayMod","MOD",cells[2]);compactKnob("delayMix","LEVEL",cells[3]);printedToggle("delayOn","ON",cells[4].withTrimmedTop(18));
  caption({},cells[0].withTrimmedTop(63).withHeight(12),-1,9);labels.getLast()->getProperties().set("live",true);
  addSync("delaySync",6,*delayTimeKnob,cells[5].withTrimmedTop(18).withHeight(45),-1,labels.getLast());
  area=section("REVERB",teal,{1322,lowerY,246,lowerH});cells=split(area,{1.8f,1,1,1,1,.75f});
  chipChoice("reverbType","",{"VINT","PLATE","SPACE"},cells[0].withHeight(65));compactKnob("reverbSize","SIZE",cells[1]);compactKnob("reverbDamp","DAMP",cells[2]);compactKnob("reverbColor","TONE",cells[3]); /* TONE: COLOR ran into LEVEL */compactKnob("reverbMix","LEVEL",cells[4]);printedToggle("reverbOn","ON",cells[5].withTrimmedTop(18));
 }
 void buildKeyboard() {
  using namespace orbitui;
  addButton(octDown, "OCT -", {left + 96, keysY + 64, 68, 28}, neutral, 0); addButton(octUp, "OCT +", {left + 96, keysY + 36, 68, 28}, neutral, 0);
  for(auto* key:{&octDown,&octUp})key->getProperties().remove("cream"); // playing keys: our dark caps with the light bar
  octDown.onClick = [this] { octave = jmax(0, octave - 1); updateKeyboard(); }; octUp.onClick = [this] { octave = jmin(4, octave + 1); updateKeyboard(); };
  // LFO TRIG: held down, it brings the LFO in when LFO MAN is on (it lights while held, from the panel or a learned MIDI button).
  addButton(lfoTrigButton, "LFO TRIG", {left + 94, keysY + 116, 72, 34}, neutral, 0);
  lfoTrigButton.getProperties().remove("cream"); // a playing key: dark cap, its light bar on while held
  for(auto* key:{&octDown,&octUp,&lfoTrigButton})key->getProperties().set("fontSize",11.f); // small legends
  lfoTrigButton.onStateChange = [this] {
   const bool down = lfoTrigButton.isDown(); if (down == lfoTrigDown) return; lfoTrigDown = down;
   auto* trig = orbit.state.getParameter("lfoTrig"); trig->beginChangeGesture(); trig->setValueNotifyingHost(down ? 1.f : 0.f); trig->endChangeGesture();
  };
  compactKnob("bendFilter","MOD.",{left+86,keysY+157,88,60}); labels.getLast()->setFont(orbitui::legend(10.5f));
  caption("PITCH",{left+10,keysY+33,62,16},-1,12); labels.getLast()->setFont(orbitui::legend(10.5f));
  pitchWheel.setName("Pitch bend");pitchWheel.getProperties().set("pitchWheel",true);
  pitchWheel.setSliderStyle(Slider::LinearVertical);pitchWheel.setTextBoxStyle(Slider::NoTextBox,false,0,0);pitchWheel.setRange(-8192,8191,1);pitchWheel.setValue(0,dontSendNotification);
  pitchWheel.setDoubleClickReturnValue(true,0);pitchWheel.setScrollWheelEnabled(false);
  pitchWheel.onValueChange=[this]{orbit.screenPitch.store(roundToInt(pitchWheel.getValue())+8192);};
  pitchWheel.onDragEnd=[this]{pitchWheel.setValue(0,sendNotificationSync);};
  place(pitchWheel,{left+10,keysY+52,62,156},-1);
  lfoTrigButton.getProperties().set("parameterId","lfoTrig");
  // CHORD MEMORY: with keys held, CHORD learns them and turns on; with no key held it turns the chord on or off.
  addToggle("hold","HOLD",-1,{left+10,keysY+2,62,30},0,orbitui::neutral);toggles.getLast()->button.getProperties().set("fontSize",11.f); // HOLD and CHORD: performance keys beside the keyboard
  addToggle("chordOn","CHORD",-1,{left+94,keysY+2,72,30},0,orbitui::neutral);chordKey=&toggles.getLast()->button;chordKey->getProperties().set("fontSize",11.f);toggles.getLast()->attachment.reset();
  chordKey->setClickingTogglesState(false);chordKey->onClick=[this]{auto* p=orbit.state.getParameter("chordOn");const bool learned=orbit.learnChord();
   p->beginChangeGesture();p->setValueNotifyingHost(learned||p->getValue()<.5f?1.f:0.f);p->endChangeGesture();};
  keyboard.setScrollButtonsVisible(false); keyboard.setKeyWidth(float(keysW) / 36.f); keyboard.setOctaveForMiddleC(4); keyboard.setBlackNoteLengthProportion(.62f);
  place(keyboard, {left + 186, keysY + 6, keysW, keysH - 6}, -1);
  // OUTPUT beside the keys: VOLUME and TUNE, where the hand finds them while playing.
  auto out=section("OUTPUT",teal,{left+contentW-outputW,keysY,outputW,keysH});
  addRowKnob("gain","VOLUME",out.withHeight(84));compactKnob("masterTune","TUNE",out.withTrimmedTop(100).withHeight(70));
  // Computer keyboard, AZERTY rows: Q S D F G H J K L M are the white keys starting on C,
  // Z E T Y U O P the black keys above them. OCT - / OCT + move the range.
  keyboard.clearKeyMappings();
  const char* keys = "qzsedftgyhujkolpm";
  for (int i = 0; keys[i] != 0; ++i) keyboard.setKeyPressForNote(KeyPress(keys[i], 0, 0), i);
  updateKeyboard();
 }
 void updateKeyboard() {
  // Release every note held from the computer keyboard or the mouse before moving the range. Otherwise a key
  // still down during the change is released as a different note, and the original one never stops.
  keyboard.focusLost(Component::focusChangedDirectly);
  keyboard.setAvailableRange(12 * (octave + 1), 12 * (octave + 1) + 60);
  keyboard.setKeyPressBaseOctave(octave + 1); surface.repaint();
 }
 void setPage(int index) { page = index; for (auto& entry : paged) entry.first->setVisible(entry.second == page); surface.repaint(); }

 // ---- painting -------------------------------------------------------------------------
 void paintSurface(Graphics& g) {
  using namespace orbitui;
  g.setGradientFill(ColourGradient(Colour(0xff111313),0,0,Colour(0xff050606),0,float(designH),false));g.fillRect(16,0,designW-32,designH); // the end strips stay clear: the editor draws the wood
  g.setTiledImageFill(panelGrain,0,0,1.f);g.fillRect(16,0,designW-32,designH);
  g.setColour(Colours::white);g.drawImage(brandLogo,Rectangle<float>(26,0,112,112));
  g.setColour(ink);g.setFont(Font(FontOptions("Helvetica Neue",43.f,Font::bold)).withExtraKerningFactor(.06f));
  g.drawText("VANTAGE",left+112,16,350,56,Justification::centredLeft);
  g.setColour(dim);g.setFont(face(12.f).withExtraKerningFactor(.10f));g.drawText("PROGRAMMABLE POLYPHONIC SYNTHESIZER",left+114,76,350,18,Justification::centredLeft);
  Rectangle<float> window(510,22,344,70);g.setColour(Colour(0xff020303));g.fillRoundedRectangle(window,3);g.setColour(Colour(0xff36382f));g.drawRoundedRectangle(window,3,1);
  g.setColour(led);g.setFont(mono(11.f));g.drawText(displayLine,522,29,320,20,Justification::centredLeft);
  g.setColour(dim);g.setFont(face(12.f));g.drawText("BANK",876,19,118,14,Justification::centred);g.drawText("PATCH",1024,19,307,14,Justification::centred);
  for(const auto& [area,colour]:wells){g.setColour(colour);g.fillRoundedRectangle(area.toFloat(),4.f);} // groups inside a section (EFFECTS: chorus, phaser)
  for(const auto& section:sections){
   if(section.page>=0&&section.page!=page)continue;
   g.setColour(ink.withAlpha(.28f));g.drawRect(section.area,1);
   auto bar=section.area.withHeight(27);g.setColour(section.colour);g.fillRect(bar);
   g.setColour(section.colour==steel?steelInk:ink);g.setFont(face(19.f,true).withExtraKerningFactor(.07f));g.drawFittedText(section.title,bar.reduced(8,0),Justification::centredLeft,1,.8f);
   g.setColour(section.colour);g.fillRect(section.area.withY(section.area.getBottom()-3).withHeight(3));
  }
  g.setColour(steel);for(const auto& rule:tagRules)g.fillRect(rule);
  g.setColour(ink.withAlpha(.2f));g.drawHorizontalLine(508,40,494); /* between VCO 1 and VCO 2 */ g.drawHorizontalLine(keysY-9,32,1568);
  g.setColour(dim);g.setFont(face(10.5f));g.drawText("C"+String(octave)+" — C"+String(octave+5),left+88,keysY+96,84,16,Justification::centred);
  g.setColour(Colour(0xff020303));g.fillRect(left+186,keysY,keysW,6);

 }

 // ---- layout export: tells tools/orbit_render.py where every control sits ----------------------
 static String colourName(Colour colour) { using namespace orbitui; return colour == amber ? "amber" : colour == steel ? "steel" : colour == teal ? "teal" : colour == bright ? "bright" : colour == dim ? "dim" : colour == ink ? "ink" : "neutral"; }
 static DynamicObject::Ptr boxed(Rectangle<int> r) { DynamicObject::Ptr item = new DynamicObject(); item->setProperty("x", r.getX()); item->setProperty("y", r.getY()); item->setProperty("w", r.getWidth()); item->setProperty("h", r.getHeight()); return item; }
 void collect(Component& parent, Array<var>& texts, Array<var>& faderList, Array<var>& knobList, Array<var>& buttonList) {
  for (auto* child : parent.getChildren()) {
   if (!child->isVisible() || child->getProperties().contains("overlay") || dynamic_cast<ComboBox*>(child) != nullptr || dynamic_cast<MidiKeyboardComponent*>(child) != nullptr) continue;
   const auto area = surface.getLocalArea(child, child->getLocalBounds()); auto item = boxed(area);
   if (auto* label = dynamic_cast<Label*>(child)) {
    if (label->getProperties().contains("live")) continue; // a value readout: it would make the layout differ from one patch to the next
    item->setProperty("text", label->getText()); item->setProperty("size", label->getFont().getHeight()); item->setProperty("colour", colourName(label->findColour(Label::textColourId))); texts.add(var(item.get()));
   } else if (auto* slider = dynamic_cast<Slider*>(child)) {
    if (slider->isRotary()) {
     const bool selector = slider->getProperties().contains("selector"); const auto travel = slider->getRotaryParameters();
     item->setProperty("cx", area.getCentreX()); item->setProperty("cy", area.getCentreY()); item->setProperty("d", jmin(area.getWidth(), area.getHeight()) - (selector ? 6 : 14));
     item->setProperty("selector", selector); item->setProperty("start", travel.startAngleRadians); item->setProperty("end", travel.endAngleRadians);
     item->setProperty("colour", colourName(slider->findColour(Slider::rotarySliderFillColourId))); knobList.add(var(item.get()));
    } else { // the cap travels between top and bottom (the look-and-feel keeps 12 px clear at each end)
     item->setProperty("cx", area.getCentreX()); item->setProperty("top", area.getY() + 12); item->setProperty("bottom", area.getBottom() - 12);
     item->setProperty("marks", int(slider->getProperties().getWithDefault("marks", 10))); faderList.add(var(item.get()));
    }
   } else if (auto* button = dynamic_cast<TextButton*>(child)) {
    const bool above = button->getProperties().contains("legendAbove");
    if (above) { auto legend = boxed(area.withY(area.getY() - 18).withHeight(14)); legend->setProperty("text", button->getButtonText()); legend->setProperty("size", 13); legend->setProperty("colour", "ink"); texts.add(var(legend.get())); }
    item->setProperty("text", button->getProperties().contains("liveText") || above ? String() : button->getButtonText()); item->setProperty("led", orbitui::ledMode(*button)); item->setProperty("dark", !button->getProperties().contains("cream")); item->setProperty("colour", colourName(button->findColour(TextButton::buttonColourId)));
    item->setProperty("live", button->getProperties().contains("liveText")); buttonList.add(var(item.get()));
   } else collect(*child, texts, faderList, knobList, buttonList);
  }
 }
 String exportLayout() { // returns layout metadata without writing outside the project
  Array<var> sectionList, texts, faderList, knobList, buttonList;
  for (const auto& section : sections) { auto item = boxed(section.area.withHeight(22)); item->setProperty("title", section.title); item->setProperty("colour", colourName(section.colour)); sectionList.add(var(item.get())); }
  collect(surface, texts, faderList, knobList, buttonList);
  for (const auto& fixed : {std::make_pair("BANK", Rectangle<int>(876, 7, 118, 14)), std::make_pair("PATCH", Rectangle<int>(1024, 7, 307, 14))}) { auto item = boxed(fixed.second); item->setProperty("text", fixed.first); item->setProperty("size", 11); item->setProperty("colour", "dim"); texts.add(var(item.get())); }
  DynamicObject::Ptr root = new DynamicObject();
  root->setProperty("width", int(designW)); root->setProperty("height", int(designH)); root->setProperty("left", int(left)); root->setProperty("contentWidth", int(contentW));
  root->setProperty("headerY", int(headerY)); root->setProperty("accentY", int(lineY));
  root->setProperty("display", var(boxed({564, 10, 290, 56}).get())); root->setProperty("keyboardBlock", var(boxed({left, keysY + 6, 132, keysH - 6}).get())); root->setProperty("keyboard", var(boxed({left + 186, keysY, contentW - 186, keysH}).get()));
  Array<var> keyList; // every key of the on-screen keyboard, so that the rendered keys sit exactly under the playable ones
  for (int note = keyboard.getRangeStart(); note <= keyboard.getRangeEnd(); ++note) {
   const auto key = keyboard.getRectangleForKey(note).translated(float(keyboard.getX()), float(keyboard.getY()));
   DynamicObject::Ptr item = new DynamicObject(); item->setProperty("x", key.getX()); item->setProperty("y", key.getY()); item->setProperty("w", key.getWidth()); item->setProperty("h", key.getHeight());
   item->setProperty("black", MidiMessage::isMidiNoteBlack(note)); keyList.add(var(item.get()));
  }
  root->setProperty("keys", keyList);
  root->setProperty("sections", sectionList); root->setProperty("labels", texts); root->setProperty("faders", faderList); root->setProperty("knobs", knobList); root->setProperty("buttons", buttonList);
  return JSON::toString(var(root.get()));
 }

 // ---- presets ---------------------------------------------------------------------------
 void refreshPresetMenu() {
  presets.clear(dontSendNotification); presets.addSectionHeading("FACTORY");
  for (int i = 0; i < orbit.getNumPrograms(); ++i) presets.addItem(orbit.getProgramName(i), i + 1);
  soundNames = orbit.soundNames(); // the sounds shipped with Orbit
  if (!soundNames.isEmpty()) {
   presets.addSeparator(); presets.addSectionHeading("SOUNDS");
   for (int i = 0; i < soundNames.size(); ++i) presets.addItem("SOUND / " + soundNames[i], 2000 + i);
  }
  userNames = orbit.userPresetNames();
  if (!userNames.isEmpty()) { presets.addSeparator(); presets.addSectionHeading("USER"); for (int i = 0; i < userNames.size(); ++i) presets.addItem("USER / " + userNames[i], 1000 + i); }
 }
 void midiMenu() {
  using orbitui::utf8;
  PopupMenu menu; const int touched = orbit.lastTouchedParameter();
  const auto nameOf = [this](int index) { return orbit.getParameters()[index]->getName(30); };
  if (orbit.isLearning()) menu.addItem(1, utf8("Annuler l'apprentissage"));
  else menu.addItem(1, utf8("Apprendre : touche un réglage, puis tourne un bouton de ton contrôleur"));
  menu.addItem(2, touched >= 0 && orbit.ccFor(touched) >= 0 ? utf8("Oublier le CC de « ") + nameOf(touched) + utf8(" »") : utf8("Oublier le CC du dernier réglage touché"), touched >= 0 && orbit.ccFor(touched) >= 0);
  menu.addItem(3, utf8("Tout oublier"));
  bool any = false;
  for (int index = 0; index < orbit.getParameters().size(); ++index) if (const int cc = orbit.ccFor(index); cc >= 0) { if (!any) menu.addSeparator(); any = true; menu.addItem(100 + index, "CC " + String(cc) + utf8("  →  ") + nameOf(index), false); }
  menu.showMenuAsync(PopupMenu::Options().withTargetComponent(&midiButton), [safe = Component::SafePointer<Editor>(this), touched](int result) {
   if (safe == nullptr) return;
   if (result == 1) { if (safe->orbit.isLearning()) safe->orbit.stopLearning(); else safe->orbit.startLearning(); }
   if (result == 2 && touched >= 0) safe->orbit.forget(touched);
   if (result == 3) safe->orbit.forgetAll();
   safe->timerCallback(); });
 }
 void showResult(const Result& result) { if (result.failed()) AlertWindow::showMessageBoxAsync(MessageBoxIconType::WarningIcon, "Preset", result.getErrorMessage()); }
 void saveDialog() {
  using orbitui::utf8;
  auto* dialog = new AlertWindow("Enregistrer un preset", utf8("Enregistrement dans Application Support/SF Instruments/Vantage. Un nom existant peut être remplacé après confirmation."), MessageBoxIconType::NoIcon);
  dialog->addTextEditor("name", orbit.userPresetName(), "Nom du preset");
  dialog->addButton("Enregistrer", 1, KeyPress(KeyPress::returnKey)); dialog->addButton("Annuler", 0, KeyPress(KeyPress::escapeKey));
  Component::SafePointer<Editor> safe(this);
  dialog->enterModalState(true, ModalCallbackFunction::create([safe, dialog](int result) {
   if (result != 1 || !safe) return;
    const String name = dialog->getTextEditorContents("name").trim();
    const auto store = [safe, name](bool replace) { if (!safe) return; const auto status = safe->orbit.saveUserPreset(name, replace); safe->showResult(status); if (status.wasOk()) safe->refreshPresetMenu(); };
    if (!safe->orbit.userPresetExists(name)) { store(false); return; }
    AlertWindow::showAsync(MessageBoxOptions().withIconType(MessageBoxIconType::QuestionIcon).withTitle("Remplacer le preset ?")
     .withMessage(utf8("Un preset « ") + name + utf8(" » existe déjà. Le remplacer par les réglages actuels ?")).withButton("Remplacer").withButton("Annuler").withAssociatedComponent(safe.getComponent()),
     [store](int choice) { if (choice == 1) store(true); });
  }), true);
 }

 // Follows state the host or a preset can change behind the panel's back.
 void timerCallback() override {
  const int program = orbit.getCurrentProgram(); const auto userName = orbit.userPresetName(), sound = orbit.soundName(); const bool user = userName.isNotEmpty() || sound.isNotEmpty();
  if (program != lastProgram) { lastProgram = program; shownBank = program / 8; }
  for (int i = 0; i < patchButtons.size(); ++i) patchButtons[i]->setToggleState(!user && shownBank * 8 + i == program, dontSendNotification);
  for (int i = 0; i < bankButtons.size(); ++i) bankButtons[i]->setToggleState(i == shownBank, dontSendNotification);
  if (sound.isNotEmpty()) { if (const int index = soundNames.indexOf(sound); index >= 0) presets.setSelectedId(2000 + index, dontSendNotification); else presets.setText("SOUND / " + sound, dontSendNotification); }
  else if (!user) presets.setSelectedId(program + 1, dontSendNotification);
  else if (const int index = userNames.indexOf(userName); index >= 0) presets.setSelectedId(1000 + index, dontSendNotification);
  else presets.setText("USER / " + userName, dontSendNotification);
  const bool eight = orbit.state.getRawParameterValue("mode")->load() > .5f;
  const String line = (sound.isNotEmpty() ? String("SOUND") : user ? String("USER") : "BANK " + String::charToString(juce_wchar('A' + program / 8)) + " / " + String(program % 8 + 1)) + (eight ? "    8 VOICES" : "    6 VOICES") + (orbit.isPatchModified() ? "    EDITED" : "");
  midiButton.setToggleState(orbit.isLearning(), dontSendNotification);
  String shown = line;
  const int steps = orbit.sequenceLength();
  if (orbit.state.getRawParameterValue("seqOvd")->load() > .5f) shown = "SEQ OVERDUB    " + String(steps) + " STEPS    QUANTIZED";
  else if (orbit.state.getRawParameterValue("seqRec")->load() > .5f) shown = "SEQ REC    STEP " + String(steps + 1) + " / 64    REST = SILENCE";
  else if (orbit.state.getRawParameterValue("seqPlay")->load() > .5f) shown = (orbit.sequenceSlot() >= 0 ? "SEQ " + String(orbit.sequenceSlot() + 1) : String("SEQ PLAY")) + "    " + String(steps) + " STEPS    " + (orbit.state.getRawParameterValue("seqLive")->load() > .5f ? "KEYS: PANEL SOUND    SEQ: ITS OWN" : "KEY = TRANSPOSE");
  sequenceEditor.refresh();
  if (chordKey != nullptr) chordKey->setToggleState(orbit.state.getRawParameterValue("chordOn")->load() > .5f, dontSendNotification);
  if (orbit.state.getRawParameterValue("chordOn")->load() > .5f) shown += "    CHORD " + orbit.chordText().replace(",", " ");
  if (recKey != nullptr) { const bool rec = orbit.state.getRawParameterValue("seqRec")->load() > .5f, ovd = orbit.state.getRawParameterValue("seqOvd")->load() > .5f;
   recKey->setToggleState(rec || ovd, dontSendNotification); recKey->setButtonText(ovd ? "OVD" : "REC");
   if (ovd != recKey->getProperties().contains("ledBlue")) { if (ovd) recKey->getProperties().set("ledBlue", true); else recKey->getProperties().remove("ledBlue"); recKey->repaint(); } }
  if (orbit.editStep.load() >= 0) shown = "SEQ EDIT    STEP " + String(orbit.editStep.load() + 1).paddedLeft('0', 2) + " / " + String(steps).paddedLeft('0', 2) + (orbit.editLane.load() ? "    OVD LINE" : "    SEQ LINE");
  if (seqStepLabel != nullptr) { const bool rec = orbit.state.getRawParameterValue("seqRec")->load() > .5f, play = orbit.state.getRawParameterValue("seqPlay")->load() > .5f;
   const String text = rec ? "REC " + String(steps + 1).paddedLeft('0', 2) : play && steps > 0 ? String(orbit.sequencePosition() + 1).paddedLeft('0', 2) + "/" + String(steps).paddedLeft('0', 2) : String(steps) + " ST";
   if (seqStepLabel->getText() != text) seqStepLabel->setText(text, dontSendNotification); }
  seqMemory.setButtonText(orbit.sequenceSlot() >= 0 ? "SEQ " + String(orbit.sequenceSlot() + 1) : String("SEQ -"));
  if (orbit.isLearning()) { const int touched = orbit.lastTouchedParameter();
   shown = touched < 0 ? String("MIDI LEARN    TOUCH A CONTROL") : "MIDI LEARN    " + orbit.getParameters()[touched]->getName(14).toUpperCase() + "  <  MOVE A CC"; }
  if (shown != displayLine) { displayLine = shown; surface.repaint(510, 22, 344, 30); }
  // Standalone only: keep the typing keys on the piano keyboard. Inside a DAW the host keeps its own shortcuts.
  if (orbit.wrapperType == AudioProcessor::wrapperType_Standalone && isShowing() && Component::getCurrentlyFocusedComponent() == nullptr && Component::getCurrentlyModalComponent() == nullptr) keyboard.grabKeyboardFocus();
  lfoTrigButton.setToggleState(orbit.state.getRawParameterValue("lfoTrig")->load() > .5f, dontSendNotification);
  for (auto* sync : syncs) { // synced: the stepped note-value knob takes the place of the free one
   const bool synced = orbit.state.getRawParameterValue(sync->id)->load() > .5f;
   sync->free->setAlpha(synced ? 0.f : 1.f); sync->free->setInterceptsMouseClicks(!synced, false);
   sync->knob->setAlpha(synced ? 1.f : 0.f); sync->knob->setInterceptsMouseClicks(synced, false);
   if (synced) sync->value->setText(sync->knob->getTextFromValue(sync->knob->getValue()), dontSendNotification);
   else if (sync->wasSynced) { if (sync->free->onValueChange) sync->free->onValueChange(); else sync->value->setText({}, dontSendNotification); }
   sync->wasSynced = synced;
  }
  const bool linked = orbit.state.getRawParameterValue("envLink")->load() > .5f;
  for (auto* fader : faders) if (fader->followsLink) fader->slider.setAlpha(linked ? .4f : 1.f); // dimmed, but still grabbable
 }

 Image brandLogo=ImageCache::getFromMemory(BinaryData::orbiticon_png,BinaryData::orbiticon_pngSize);
 Image panelGrain = [] {
  Image tile(Image::ARGB, 256, 256, true); Random random(106);
  for (int y = 0; y < 256; ++y) for (int x = 0; x < 256; ++x)
   tile.setPixelAt(x, y, (random.nextBool() ? Colours::white : Colours::black).withAlpha(float(random.nextInt(3)) / 255.f));
  return tile;
 }();
 Orbit& orbit; OrbitSkin skin; OrbitLook look; Surface surface; PianoKeyboard keyboard;
 std::vector<std::pair<Rectangle<int>,Colour>> wells;std::vector<Section> sections; std::vector<std::pair<Component*, int>> paged;
 // SEQUENCE EDIT: the steps as a grid. Click a step (or the + after the last one to add a step), then play a note or
 // a chord on the keys: it replaces the step (ADD: joins it) and the next step is selected. REST, INSERT, DELETE edit
 // the selected step. The audio side does the replacing, so the keys are heard while editing.
 struct SequenceEditor : Component {
  explicit SequenceEditor(Orbit& o):orbit(o){
   for(auto* b:{&prev,&next,&add,&rest,&insert,&remove,&clearOvd,&mergeOvd,&done}){addAndMakeVisible(*b);b->setWantsKeyboardFocus(false);b->getProperties().set("fontSize",12.f);}
   add.setClickingTogglesState(true);setWantsKeyboardFocus(false);
   prev.onClick=[this]{select(orbit.editStep.load()-1,lane());};next.onClick=[this]{select(orbit.editStep.load()+1,lane());};
   add.onClick=[this]{orbit.editAdds.store(add.getToggleState());repaint();};
   rest.onClick=[this]{const int i=orbit.editStep.load();const bool l=lane();orbit.editSequence([i,l](Arpeggiator::Sequence& q){q.clearLane(i,l);});};
   insert.onClick=[this]{const int i=orbit.editStep.load();orbit.editSequence([i](Arpeggiator::Sequence& q){q.insertAfter(i);});select(i+1,lane());};
   remove.onClick=[this]{const int i=orbit.editStep.load();orbit.editSequence([i](Arpeggiator::Sequence& q){q.remove(i);});select(i,lane());};
   clearOvd.onClick=[this]{orbit.editSequence([](Arpeggiator::Sequence& q){q.clearOverdub();});};
   mergeOvd.onClick=[this]{orbit.editSequence([](Arpeggiator::Sequence& q){q.mergeOverdub();});};
   done.onClick=[this]{if(onClose)onClose();};
  }
  bool lane() const {return orbit.editLane.load();}
  void open(){if(orbit.sequenceLength()==0)orbit.editSequence([](Arpeggiator::Sequence& q){q.insertAfter(-1);});orbit.editAdds.store(add.getToggleState());select(jmax(0,orbit.editStep.load()),lane());setVisible(true);toFront(false);}
  void close(){orbit.editStep.store(-1);setVisible(false);}
  void select(int i,bool overdubLine){orbit.editLane.store(overdubLine);orbit.editStep.store(jlimit(0,jmax(0,orbit.sequenceLength()-1),i));refresh(true);}
  void refresh(bool force=false){if(!isVisible())return;const auto text=orbit.sequenceText();const int at=orbit.editStep.load()*2+(lane()?1:0);
   if(force||text!=shownText||at!=shownStep){shownText=text;shownStep=at;sequence=orbit.sequenceCopy();repaint();}}
  // 16 steps a row, each step two cells: the recorded notes (SEQ) over the overdubbed ones (OVD, blue).
  Rectangle<int> stepArea(int i) const {const int w=(getWidth()-64)/16;return {52+(i%16)*w,40+(i/16)*66,w-4,62};}
  Rectangle<int> laneCell(int i,bool dub) const {auto r=stepArea(i);return dub?r.withTrimmedTop(32):r.withHeight(30);}
  void resized() override {auto row=Rectangle<int>(12,getHeight()-46,getWidth()-24,34);
   for(auto* b:{&prev,&next,&add,&rest,&insert,&remove,&clearOvd,&mergeOvd})b->setBounds(row.removeFromLeft(118).reduced(4,0));done.setBounds(row.removeFromRight(110).reduced(4,0));}
  void paint(Graphics& g) override {using namespace orbitui;const Colour blue(0xff6cc4ff);
   g.setColour(Colour(0xff0b0c0d));g.fillRoundedRectangle(getLocalBounds().toFloat(),6);g.setColour(teal);g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1),6,2);
   g.setColour(ink);g.setFont(face(18.f,true));g.drawText("SEQUENCE EDIT",14,8,240,24,Justification::centredLeft);
   g.setColour(dim);g.setFont(legend(13.f));
   g.drawText("Click a SEQ or OVD cell, then play a note or chord: it "+String(add.getToggleState()?"joins":"replaces")+" that line of the step.   + adds a step.",260,8,getWidth()-280,24,Justification::centredLeft);
   const int rows=jmin(64,sequence.length+1)/16+1;
   for(int r=0;r<rows&&r<4;++r){g.setFont(face(11.f,true));g.setColour(ink);g.drawText("SEQ",8,40+r*66,40,30,Justification::centredLeft);g.setColour(blue);g.drawText("OVD",8,72+r*66,40,30,Justification::centredLeft);}
   for(int i=0;i<jmin(64,sequence.length+1);++i){
    if(i==sequence.length){const auto r=stepArea(i).toFloat();g.setColour(dim.withAlpha(.5f));g.drawRoundedRectangle(r,4,1);g.setFont(face(22.f));g.drawText("+",r,Justification::centred);continue;}
    String played,dubbed;for(int k=0;k<sequence.count[size_t(i)];++k)(sequence.dubbed[size_t(i)][size_t(k)]?dubbed:played)<<MidiMessage::getMidiNoteName(sequence.notes[size_t(i)][size_t(k)],true,true,4)<<" ";
    for(const bool dub:{false,true}){const auto r=laneCell(i,dub).toFloat();const bool selected=shownStep==i*2+(dub?1:0);
     g.setColour(selected?(dub?Colour(0xff1d3a52):Colour(0xff2a4a5a)):Colour(0xff17191b));g.fillRoundedRectangle(r,4);
     g.setColour(selected?(dub?blue:led):Colour(0xff34373a));g.drawRoundedRectangle(r,4,selected?2.f:1.f);
     if(!dub){g.setColour(dim);g.setFont(mono(9.f));g.drawText(String(i+1).paddedLeft('0',2),r.reduced(4,2),Justification::topLeft);}
     const auto& text=dub?dubbed:played;g.setFont(face(12.f));
     if(text.isEmpty()){g.setColour(dim.withAlpha(.6f));g.drawText(dub?String("-"):String("REST"),r,Justification::centred);}
     else{g.setColour(dub?blue:ink);g.drawFittedText(text.trim(),r.reduced(4,1).withTrimmedLeft(dub?0:12).toNearestInt(),Justification::centred,1,.55f);}}}
  }
  void mouseDown(const MouseEvent& e) override {
   for(int i=0;i<jmin(64,sequence.length+1);++i){
    if(i==sequence.length&&stepArea(i).contains(e.getPosition())){orbit.editSequence([](Arpeggiator::Sequence& q){q.insertAfter(q.length-1);});select(i,false);return;}
    for(const bool dub:{false,true})if(i<sequence.length&&laneCell(i,dub).contains(e.getPosition())){select(i,dub);return;}}}
  Orbit& orbit;TextButton prev{"<  STEP"},next{"STEP  >"},add{"ADD"},rest{"CLEAR"},insert{"INSERT"},remove{"DELETE"},clearOvd{"CLEAR OVD"},mergeOvd{"MERGE OVD"},done{"DONE"};
  std::function<void()> onClose;Arpeggiator::Sequence sequence;String shownText;int shownStep=-2;
 };
 void showSequenceEditor(bool show){if(show)sequenceEditor.open();else sequenceEditor.close();}
 // A clickable wave name around a SHAPE knob.
 struct ShapeStop : Component { ShapeStop(Slider& s,float v):slider(s),value(v){setMouseCursor(MouseCursor::PointingHandCursor);setRepaintsOnMouseActivity(false);}
  void mouseDown(const MouseEvent&) override {slider.setValue(value,sendNotificationSync);} Slider& slider;float value; };
SequenceEditor sequenceEditor{orbit};
 OwnedArray<ShapeStop> shapeStops;Button* restKey=nullptr;bool restDown=false;TextButton seqMemory,seqEditKey;Button* recKey=nullptr;Label* seqStepLabel=nullptr;
 std::vector<Rectangle<float>> tagRules; OwnedArray<PanelChoice> panelChoices; OwnedArray<Label> labels; OwnedArray<Fader> faders; OwnedArray<Toggle> toggles; OwnedArray<ChoiceButtons> choices; OwnedArray<WaveKnob> waveKnobs; OwnedArray<SlideSwitch> switches; OwnedArray<ChoiceMenuButton> menuButtons; Slider* delayTimeKnob = nullptr;
 struct Sync { std::unique_ptr<SyncKey> key; std::unique_ptr<SyncKnob> knob; Slider* free = nullptr; Label* value = nullptr; const char* id = ""; bool wasSynced = true; };
 OwnedArray<Sync> syncs;
 OwnedArray<TextButton> bankButtons, patchButtons;
 Slider pitchWheel; ComboBox presets; TextButton saveButton, loadButton, octDown, octUp, midiButton, lfoTrigButton; bool lfoTrigDown = false; void* forceMonitor = nullptr; int sentPressure = 0;
 StringArray userNames, soundNames; String displayLine; Colour sectionColour{orbitui::neutral};
 int page = 0, octave = 2, shownBank = 0, lastProgram = -1;
};
