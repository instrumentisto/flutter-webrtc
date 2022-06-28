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
  s.source_files     = 'Classes/**/*'
  s.dependency 'FlutterMacOS'

  s.platform = :osx, '10.11'
  s.pod_target_xcconfig = { 'DEFINES_MODULE' => 'YES' }
  s.swift_version = '5.0'
end
