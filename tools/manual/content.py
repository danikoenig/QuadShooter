try:
    from lib import install_text
except ImportError:
    pass

CONTENT = {
"title": "QUAD SHOOTER", "file": "QuadShooter", "repo": "QuadShooter",
"accent": "#4fb8ee", "accent2": "#8a6cf0",
"en": {
 "subtitle": "4-slot sample player",
 "kind": "Instrument for macOS (Apple Silicon)  \xb7  VST3, Audio Unit and CLAP",
 "by": "by Dani Koenig",
 "intro": ("Quad Shooter holds four samples (A, B, C, D) and blends them with one dot on an XY pad. Each slot has its own colour, waveform, mute and fine pitch. "
           "Layer a kick, a sub, a click and a noise hit and move the dot to change the mix. Delay, reverb and clipper follow the amp, and a limiter guards the output. "
           "Sister plugin of One Shooter."),
 "note": "Key Sync pitches the tonal slots to one lead slot, so layered tones stay in tune.",
 "blocks": [],
 "quick_title": "Quick start",
 "quick": [
   ("1. Load", "Drag a sample onto a slot (A to D) or double-click a sample in the library to load it into the selected slot."),
   ("2. Blend", "Move the dot in the <b>XY Mix</b>. A corner plays one slot, the centre plays all four equally."),
   ("3. Edit", "Select a slot. The edit row below shows its start, end, level, pan, envelope, pitch, tone, cuts and velocity."),
   ("4. Tune", "Pick a lead slot and switch on <b>Key Sync</b>: the other tonal slots follow its note."),
 ],
 "first": "For a layered kick: A = kick, B = sub sine, C = click. XY Mix towards A and B, Key Sync on, clipper with Auto Gain.",
 "sections": [
  ("#4fb8ee", "SLOTS A, B, C, D", [
    ("Slot", "Own colour, waveform, MUTE and a fine PITCH slider (plus or minus 12 semitones in 1-cent steps, Shift = 0.2 cent)."),
    ("Pitch detection", "Every sample is analysed. Claps, hats and noise are marked as no pitch."),
    ("Root note", "Click the note under a waveform for the root menu, double-click to type it (F#2, Eb1)."),
    ("Prev / Next", "Step through the library for the selected slot."),
  ]),
  ("#e0a800", "EDIT ROW (selected slot)", [
    ("Sample START / END", "Trim the sample."),
    ("LEVEL, PAN", "Volume and position of the slot."),
    ("HOLD, DECAY", "Envelope of the slot."),
    ("PITCH", "Pitch of the slot."),
    ("TONE", "Darker to brighter."),
    ("CUT LOW / HIGH", "High-pass and low-pass."),
    ("VELSENS", "How much key velocity changes the level."),
  ]),
  ("#1fb3c8", "XY MIX AND KEY SYNC", [
    ("XY Mix", "Blends four slots with one dot, equal power. Corner = one slot, centre = all four equal."),
    ("Key Sync", "Lead slot sets the note. Nearest: same note in the closest octave (keeps each sample's character). Exact: exactly the same note."),
    ("Chromatic", "Plays all slots across the keyboard (C3 = original pitch), polyphonic."),
  ]),
  ("#8a6cf0", "AMP AND FX", [
    ("AMP", "ATTACK, DECAY and RELEASE for all slots. Release Off = pure one-shot."),
    ("DELAY", "Mono or Ping-Pong, synced (1/64 to 1/1, dotted, triplets) or free in ms, FEEDBACK, MIX."),
    ("REVERB", "SIZE from a tight drum room to a big space for pads, DAMP, MIX."),
    ("CLIPPER", "Knack-style, soft or hard, 4x oversampling, with AUTO GAIN so loudness stays matched."),
    ("Order", "The FX chain sits after the amp. Change the order with the arrows."),
    ("OUTPUT", "GAIN into a limiter (ceiling -1 dB) with gain reduction meter."),
  ]),
  ("#ff7a2f", "LIBRARY", [
    ("Add folders", "Drop a folder. All one-shots are indexed, the files stay where they are."),
    ("Use", "Click = listen. Drag a sample onto a slot = load. Double-click = load into the selected slot."),
    ("Import", "Takes over the One Shooter index on first start."),
  ]),
 ],
 "recipes": [
   ("Layered kick", "A kick, B sub sine, C short click. Key Sync on, Nearest. XY Mix between A and B. Clipper 25 % with Auto Gain."),
   ("Snare stack", "A snare, B clap, C noise tail. XY Mix near the centre. Reverb small, MIX 15 %."),
   ("Morphing hit", "Four different hits, automate the XY Mix over a bar for a hit that changes."),
   ("Chord layer", "Chromatic on, load four chord tones with Key Sync Exact."),
 ],
 "tips": [
   "All parameters can be automated, including the XY position.",
   "Samples are stored by file path. If you move a file, load it again.",
   "With Release Off a slot plays out as a pure one-shot.",
   "One Shooter is the single-slot sister plugin with pitch envelope and sub.",
 ],
 "install": "",
},
"de": {
 "subtitle": "Sample-Player mit 4 Slots",
 "kind": "Instrument für macOS (Apple Silicon)  \xb7  VST3, Audio Unit und CLAP",
 "by": "von Dani Koenig",
 "intro": ("Quad Shooter hält vier Samples (A, B, C, D) und mischt sie mit einem Punkt auf einem XY-Pad. Jeder Slot hat eigene Farbe, Wellenform, Mute und Feinstimmung. "
           "Schichte Kick, Sub, Click und einen Noise-Hit und bewege den Punkt, um den Mix zu ändern. Delay, Reverb und Clipper folgen dem Amp, ein Limiter bewacht den Ausgang. "
           "Schwester-Plugin von One Shooter."),
 "note": "Key Sync stimmt die tonalen Slots auf einen Lead-Slot, damit geschichtete Töne zusammenpassen.",
 "blocks": [],
 "quick_title": "Schnellstart",
 "quick": [
   ("1. Laden", "Ein Sample auf einen Slot (A bis D) ziehen oder in der Library doppelklicken, um es in den gewählten Slot zu laden."),
   ("2. Mischen", "Den Punkt im <b>XY Mix</b> bewegen. Eine Ecke spielt einen Slot, die Mitte spielt alle vier gleich laut."),
   ("3. Bearbeiten", "Einen Slot wählen. Die Edit-Zeile darunter zeigt Start, Ende, Level, Pan, Hüllkurve, Pitch, Tone, Cuts und Velocity."),
   ("4. Stimmen", "Einen Lead-Slot wählen und <b>Key Sync</b> einschalten: Die anderen tonalen Slots folgen seiner Note."),
 ],
 "first": "Für eine geschichtete Kick: A = Kick, B = Sub-Sinus, C = Click. XY Mix Richtung A und B, Key Sync an, Clipper mit Auto Gain.",
 "sections": [
  ("#4fb8ee", "SLOTS A, B, C, D", [
    ("Slot", "Eigene Farbe, Wellenform, MUTE und ein PITCH-Feinregler (plus oder minus 12 Halbtöne in 1-Cent-Schritten, Shift = 0,2 Cent)."),
    ("Pitch-Erkennung", "Jedes Sample wird analysiert. Claps, Hats und Rauschen werden als ohne Tonhöhe erkannt."),
    ("Root-Note", "Die Note unter der Wellenform anklicken für das Root-Menü, doppelklicken zum Eintippen (F#2, Eb1)."),
    ("Prev / Next", "Blättert durch die Library für den gewählten Slot."),
  ]),
  ("#e0a800", "EDIT-ZEILE (gewählter Slot)", [
    ("Sample START / END", "Sample zuschneiden."),
    ("LEVEL, PAN", "Lautstärke und Position des Slots."),
    ("HOLD, DECAY", "Hüllkurve des Slots."),
    ("PITCH", "Tonhöhe des Slots."),
    ("TONE", "Dunkler bis heller."),
    ("CUT LOW / HIGH", "Hochpass und Tiefpass."),
    ("VELSENS", "Wie stark die Anschlagstärke den Pegel verändert."),
  ]),
  ("#1fb3c8", "XY MIX UND KEY SYNC", [
    ("XY Mix", "Mischt vier Slots mit einem Punkt, gleiche Leistung. Ecke = ein Slot, Mitte = alle vier gleich."),
    ("Key Sync", "Der Lead-Slot bestimmt die Note. Nearest: gleiche Note in der nächsten Oktave (erhält den Charakter jedes Samples). Exact: genau dieselbe Note."),
    ("Chromatic", "Spielt alle Slots über die Tastatur (C3 = Originaltonhöhe), polyphon."),
  ]),
  ("#8a6cf0", "AMP UND FX", [
    ("AMP", "ATTACK, DECAY und RELEASE für alle Slots. Release Off = reiner One-Shot."),
    ("DELAY", "Mono oder Ping-Pong, synchron (1/64 bis 1/1, punktiert, triolisch) oder frei in ms, FEEDBACK, MIX."),
    ("REVERB", "SIZE von engem Drum-Raum bis grossem Raum für Pads, DAMP, MIX."),
    ("CLIPPER", "Im Knack-Stil, soft oder hard, 4x Oversampling, mit AUTO GAIN, damit die Lautheit gleich bleibt."),
    ("Reihenfolge", "Die FX-Kette liegt nach dem Amp. Mit den Pfeilen änderst du die Reihenfolge."),
    ("OUTPUT", "GAIN in einen Limiter (Ceiling -1 dB) mit Gain-Reduction-Anzeige."),
  ]),
  ("#ff7a2f", "LIBRARY", [
    ("Ordner hinzufügen", "Ordner hineinziehen. Alle One-Shots werden indexiert, die Dateien bleiben wo sie sind."),
    ("Benutzen", "Klick = anhören. Sample auf einen Slot ziehen = laden. Doppelklick = in den gewählten Slot laden."),
    ("Import", "Übernimmt beim ersten Start den Index von One Shooter."),
  ]),
 ],
 "recipes": [
   ("Geschichtete Kick", "A Kick, B Sub-Sinus, C kurzer Click. Key Sync an, Nearest. XY Mix zwischen A und B. Clipper 25 % mit Auto Gain."),
   ("Snare-Stack", "A Snare, B Clap, C Noise-Tail. XY Mix nahe der Mitte. Reverb klein, MIX 15 %."),
   ("Morphender Hit", "Vier verschiedene Hits, XY Mix über einen Takt automatisieren: ein Hit, der sich verändert."),
   ("Akkord-Layer", "Chromatic an, vier Akkordtöne laden, Key Sync Exact."),
 ],
 "tips": [
   "Alle Parameter lassen sich automatisieren, auch die XY-Position.",
   "Samples werden per Dateipfad gespeichert. Wenn du eine Datei verschiebst, lade sie neu.",
   "Mit Release Off spielt ein Slot als reiner One-Shot aus.",
   "One Shooter ist das Schwester-Plugin mit einem Slot, Pitch-Hüllkurve und Sub.",
 ],
 "install": "",
},
}
for _l in ("en", "de"):
    CONTENT[_l]["install"] = install_text(_l, "QuadShooter", "QuadShooter", "Quad Shooter")
