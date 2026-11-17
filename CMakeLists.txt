cmake_minimum_required(VERSION 3.22)
if(POLICY CMP0131)
  cmake_policy(SET CMP0131 NEW) # LINK_ONLY keeps plugin wrapper defines out of the test host
endif()
# Set the deployment target before compiler detection; an existing empty cache entry
# otherwise makes the SDK version become the minimum supported macOS.
if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS" FORCE)
endif()
project(Vantage VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
add_subdirectory(vendor/JUCE-8.0.6 EXCLUDE_FROM_ALL)
# The app icon is made when CMake configures: changing the image must re-run that step.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/Assets/orbit-icon.png")
juce_add_plugin(Vantage ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Source/Assets/orbit-icon.png" COMPANY_NAME "SF Instruments" BUNDLE_ID "com.sfinstruments.vantage" PLUGIN_MANUFACTURER_CODE SfIn PLUGIN_CODE Vntg FORMATS AU VST3 Standalone PRODUCT_NAME "Vantage" IS_SYNTH TRUE NEEDS_MIDI_INPUT TRUE NEEDS_MIDI_OUTPUT FALSE IS_MIDI_EFFECT FALSE COPY_PLUGIN_AFTER_BUILD FALSE VST3_CATEGORIES Instrument Synth)
juce_add_binary_data(OrbitReference SOURCES design/neo-vintage/reference-approved.png Source/Assets/orbit-icon.png
  Source/Assets/skin/knob.png Source/Assets/skin/fader.png Source/Assets/skin/led.png
  Source/Assets/skin/index_amber.png Source/Assets/skin/index_teal.png Source/Assets/skin/index_neutral.png) # our rendered knob, fader cap and LED
target_link_libraries(Vantage PRIVATE OrbitReference)
target_sources(Vantage PRIVATE Source/Plugin.cpp)
if(APPLE)
  target_sources(Vantage PRIVATE Source/ForceTouch.mm) # trackpad force on the panel keyboard
endif()
target_compile_definitions(Vantage PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(Vantage PRIVATE juce::juce_audio_utils juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)
juce_add_console_app(OrbitSmoke PRODUCT_NAME "Orbit Smoke Tests")
target_sources(OrbitSmoke PRIVATE Tests/Smoke.cpp)
target_link_libraries(OrbitSmoke PRIVATE "$<LINK_ONLY:Vantage>" juce::juce_audio_utils juce::juce_dsp)
target_compile_definitions(OrbitSmoke PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
enable_testing()
add_test(NAME audio_smoke COMMAND OrbitSmoke)

target_compile_definitions(Vantage PRIVATE ORBIT_PROJECT_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
