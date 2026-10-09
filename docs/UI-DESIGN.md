# Interface style

TelePepper uses one Horizon-inspired palette across the native OpenXR dashboard, Quest connection settings and Pepper tablet. There is no theme selector; older saved theme choices are ignored.

- Charcoal background #1A1A1A, cards #333333, button surfaces #414348.
- Primary text #EBEBEB, secondary text #B4B4B4, selected controls and accents #0064E0.
- Rounded cards and pill controls; filled Layout and Help icons with hover labels and 48-pixel dashboard hit areas.
- Camera images, depth measurements, LED swatches, emoji and thermal indicators retain their data colours. STOP retains red and selected button captions remain white.
- Shared Android typography uses system Roboto. VR retains its existing bundled font and hierarchy. Meta Optimistic Display is not bundled.

References consulted: [Meta colour guidance](https://developers.meta.com/vr/design/styles_color/), [Meta fonts and icons](https://developers.meta.com/vr/design/fonts-icons/), and the [official Meta Quest web template palette](https://github.com/meta-quest/2d-web-v0-template/blob/main/app/globals.css). This is an adaptation for TelePepper, not a replacement with a Meta UI SDK.
