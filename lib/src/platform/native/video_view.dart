import 'dart:math';

import 'package:flutter/material.dart';

import 'package:medea_flutter_webrtc/src/api/peer.dart';
import '../video_renderer.dart';

class VideoView extends StatelessWidget {
  VideoView(
    this._renderer, {
    Key? key,
    this.objectFit = VideoViewObjectFit.contain,
    this.mirror = false,
    this.enableContextMenu = true,
    this.filterQuality = FilterQuality.low,
  }) : super(key: key) {
    autoRotate = isDesktop;
  }

  final VideoRenderer _renderer;
  final VideoViewObjectFit objectFit;
  final bool mirror;
  final bool enableContextMenu;
  final FilterQuality filterQuality;
  late final bool autoRotate;

  NativeVideoRenderer get videoRenderer => _renderer as NativeVideoRenderer;

  @override
  Widget build(BuildContext context) {
    return LayoutBuilder(
        builder: (BuildContext context, BoxConstraints constraints) =>
            _buildVideoView(constraints));
  }

  Widget _buildVideoView(BoxConstraints constraints) {
    return Center(
      child: SizedBox(
        width: constraints.maxWidth,
        height: constraints.maxHeight,
        child: FittedBox(
          clipBehavior: Clip.hardEdge,
          fit: objectFit == VideoViewObjectFit.contain
              ? BoxFit.contain
              : BoxFit.cover,
          child: Center(
            child: ValueListenableBuilder<RTCVideoValue>(
              valueListenable: videoRenderer,
              builder:
                  (BuildContext context, RTCVideoValue value, Widget? child) {
                Widget result = SizedBox(
                  width: constraints.maxHeight * value.aspectRatio,
                  height: constraints.maxHeight,
                  child: child,
                );
                if (autoRotate) {
                  result = RotatedBox(
                    quarterTurns: value.quarterTurnsRotation,
                    child: result,
                  );
                }
                return Transform(
                    transform: Matrix4.identity()..rotateY(mirror ? -pi : 0.0),
                    alignment: FractionalOffset.center,
                    child: result);
              },
              child: videoRenderer.textureId != null &&
                      videoRenderer.srcObject != null
                  ? Texture(
                      textureId: videoRenderer.textureId!,
                      filterQuality: filterQuality,
                    )
                  : Container(),
            ),
          ),
        ),
      ),
    );
  }
}
