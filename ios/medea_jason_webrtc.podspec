#
# To learn more about a Podspec see http://guides.cocoapods.org/syntax/podspec.html.
# Run `pod lib lint medea_jason_webrtc.podspec` to validate before publishing.
#
Pod::Spec.new do |s|
  s.name             = 'medea_jason_webrtc'
  s.version          = '0.8.0-dev'
  s.summary          = 'Flutter WebRTC plugin based on Google WebRTC'
  s.description      = <<-DESC
Flutter WebRTC plugin based on Google WebRTC.
                       DESC
  s.homepage         = 'https://github.com/instrumentisto/flutter-webrtc'
  s.license          = { :file => '../LICENSE' }
  s.author           = { 'Instrumentisto Team' => 'developer@instrumentisto.com' }
  s.source           = { :path => '.' }
  s.source_files = 'Classes/**/*'
  s.dependency 'Flutter'
  s.platform = :ios, '9.0'

  # Flutter.framework does not contain a i386 slice.
  s.pod_target_xcconfig = { 'DEFINES_MODULE' => 'YES', 'EXCLUDED_ARCHS[sdk=iphonesimulator*]' => 'i386' }
  s.swift_version = '5.0'
end
