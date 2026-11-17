#pragma once
#include <juce_core/juce_core.h>
#include <filesystem>

class PresetStorage {
public:
 // folder: "User" for the presets saved with SAVE, "Sounds" for the sounds shipped with Orbit.
 explicit PresetStorage(juce::File project,juce::String folder="User"):root(std::move(project)),sub(std::move(folder)){}
 juce::File directory() const {return root.getChildFile("Presets").getChildFile(sub);}
 juce::Result checkDirectory() const {
  namespace fs=std::filesystem;std::error_code error;
  for(auto path:{root.getChildFile("Presets"),directory()}){
   if(fs::is_symlink(fs::path(path.getFullPathName().toStdString()),error))return juce::Result::fail(juce::String::fromUTF8("Le dossier de presets ne doit pas être un lien symbolique."));
   if(path.exists()&&!path.isDirectory())return juce::Result::fail("Le chemin des presets n'est pas un dossier.");
  }
  return juce::Result::ok();
 }
 static bool validName(const juce::String& name){return name.isNotEmpty() && name.length()<=64 && name==name.trim() && name.containsOnly("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 -_");}
 bool exists(const juce::String& name) const {return validName(name)&&directory().getChildFile(name+".orbitpreset").existsAsFile();}
 // replace: overwrite an existing preset of that name (asked for explicitly); otherwise an existing name is refused.
 juce::Result save(const juce::String& name,const juce::XmlElement& xml,bool replace=false) const {
  if(!validName(name))return juce::Result::fail(juce::String::fromUTF8("Nom : 1 à 64 lettres sans accents, chiffres, espaces, tirets ou underscores."));
  auto check=checkDirectory();if(check.failed())return check;
  auto file=directory().getChildFile(name+".orbitpreset");
  std::error_code error;const bool link=std::filesystem::is_symlink(file.getFullPathName().toStdString(),error);if(link||(file.exists()&&!(replace&&file.existsAsFile())))return juce::Result::fail(juce::String::fromUTF8("Ce nom existe déjà. Choisissez un nouveau nom pour conserver les deux versions."));
  auto created=directory().createDirectory();if(created.failed())return created;
  if(!file.replaceWithText(xml.toString()))return juce::Result::fail(juce::String::fromUTF8("Impossible d'écrire le preset dans le projet."));
  return juce::Result::ok();
 }
 std::unique_ptr<juce::XmlElement> load(const juce::String& name) const {
  if(!validName(name)||checkDirectory().failed())return {};
  auto file=directory().getChildFile(name+".orbitpreset");std::error_code error;
  if(std::filesystem::is_symlink(file.getFullPathName().toStdString(),error)||file.getSize()>1024*1024)return {};
  auto xml=juce::XmlDocument::parse(file);if(!xml||!xml->hasTagName("OrbitState"))return {};
  return xml;
 }
 juce::StringArray names() const {
  juce::StringArray result;if(checkDirectory().failed())return result;
  for(const auto& file:directory().findChildFiles(juce::File::findFiles,false,"*.orbitpreset")){auto name=file.getFileNameWithoutExtension();if(validName(name))result.add(name);}
  result.sort(true);return result;
 }
private:juce::File root;juce::String sub;
};
