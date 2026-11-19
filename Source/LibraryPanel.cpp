#include "LibraryPanel.h"
#include "Colours.h"
#include <map>

using namespace ShooterColours;

namespace
{
    // Importierter Wurzelordner = erstes Segment des relativen Pfads
    juce::String rootOf (const juce::String& folder)
    {
        return folder.upToFirstOccurrenceOf ("/", false, false);
    }

    void drawFolderIcon (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
    {
        juce::Path p;
        const float x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight();
        p.startNewSubPath (x, y + h * 0.2f);
        p.lineTo (x + w * 0.38f, y + h * 0.2f);
        p.lineTo (x + w * 0.5f, y + h * 0.36f);
        p.lineTo (x + w, y + h * 0.36f);
        p.lineTo (x + w, y + h);
        p.lineTo (x, y + h);
        p.closeSubPath();
        g.setColour (c);
        g.fillPath (p);
    }
}

LibraryPanel::LibraryPanel (QuadShooterProcessor& p) : processor (p)
{
    search.setTextToShowWhenEmpty ("Search all (e.g. kick 909)", textDim);
    search.setFont (juce::FontOptions (13.0f));
    search.setColour (juce::TextEditor::backgroundColourId, inset);
    search.setColour (juce::TextEditor::outlineColourId, cardBorder);
    search.setColour (juce::TextEditor::focusedOutlineColourId, text.withAlpha (0.4f));
    search.setColour (juce::TextEditor::textColourId, text);
    search.setIndents (8, 6);
    search.onTextChange = [this] { rebuild(); };
    addAndMakeVisible (search);

    list.setRowHeight (38);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    list.setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    list.getViewport()->setScrollBarThickness (6);
    list.setWantsKeyboardFocus (true);
    addAndMakeVisible (list);

    upButton.onClick = [this] { goUp(); };
    upButton.setTooltip ("Up one folder");
    addAndMakeVisible (upButton);

    addButton.onClick = [this] { chooseFolder(); };
    clearButton.onClick = [this] { processor.getLibrary().clear(); };
    addAndMakeVisible (addButton);
    addAndMakeVisible (clearButton);

    status.setFont (juce::FontOptions (11.0f));
    status.setColour (juce::Label::textColourId, textDim);
    addAndMakeVisible (status);

    processor.getLibrary().addChangeListener (this);
    all = processor.getLibrary().getEntries();
    revealLoaded();
    rebuild();
}

LibraryPanel::~LibraryPanel()
{
    processor.getLibrary().removeChangeListener (this);
}

//==============================================================================
void LibraryPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (card);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (cardBorder);
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);

    g.setColour (slot (active()));
    g.fillEllipse (14.0f, 17.0f, 7.0f, 7.0f);
    g.setColour (text);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("LIBRARY", 28, 12, 120, 18, juce::Justification::centredLeft);
    g.setColour (slot (active()));
    g.drawText (juce::String::fromUTF8 ("\xe2\x86\x92 SLOT ") + slotLetter (active()), 90, 12, 80, 18,
                juce::Justification::centredLeft);
    g.setColour (textDim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (juce::String ((int) all.size()) + " one-shots", getWidth() - 130, 12, 116, 18,
                juce::Justification::centredRight);

    // Breadcrumb
    g.setColour (inset);
    g.fillRoundedRectangle (crumbArea.toFloat(), 6.0f);
    const bool searching = search.getText().isNotEmpty();
    const juce::String crumb = searching ? juce::String ("Search results")
                                         : (currentPath.isEmpty() ? juce::String ("All folders")
                                                                  : currentPath);
    g.setColour (searching || currentPath.isEmpty() ? textDim : text);
    g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    g.drawText (crumb, crumbArea.withTrimmedLeft (10).withTrimmedRight (6), juce::Justification::centredLeft, true);

    if (all.empty())
    {
        auto hint = getLocalBounds().reduced (24).withTrimmedTop (120).withTrimmedBottom (70);
        g.setColour (textDim);
        g.setFont (juce::FontOptions (13.0f));
        g.drawFittedText ("Drop a folder with samples here.\n\nAll one-shots inside it\n(including subfolders)\nare found and indexed.\nYour files stay where they are.",
                          hint, juce::Justification::centredTop, 8);
    }
}

void LibraryPanel::resized()
{
    auto r = getLocalBounds().reduced (10);
    r.removeFromTop (28);
    search.setBounds (r.removeFromTop (30));
    r.removeFromTop (8);

    auto crumb = r.removeFromTop (28);
    upButton.setBounds (crumb.removeFromLeft (28));
    crumb.removeFromLeft (6);
    crumbArea = crumb;
    r.removeFromTop (6);

    auto bottom = r.removeFromBottom (54);
    status.setBounds (bottom.removeFromBottom (20));
    bottom.removeFromBottom (4);
    addButton.setBounds (bottom.removeFromLeft (bottom.getWidth() / 2 - 4));
    bottom.removeFromLeft (8);
    clearButton.setBounds (bottom);

    r.removeFromBottom (6);
    list.setBounds (r);
}

void LibraryPanel::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= (int) items.size())
        return;
    const auto& it = items[(size_t) row];

    auto r = juce::Rectangle<float> (0.0f, 1.0f, (float) w - 2.0f, (float) h - 2.0f);
    if (selected)
    {
        g.setColour (slot (active()).withAlpha (0.16f));
        g.fillRoundedRectangle (r, 6.0f);
    }
    else if (row % 2 == 0)
    {
        g.setColour (juce::Colours::white.withAlpha (0.02f));
        g.fillRoundedRectangle (r, 6.0f);
    }

    if (it.type != Item::sample)
    {
        drawFolderIcon (g, { 12.0f, (float) h * 0.5f - 7.0f, 18.0f, 14.0f },
                        it.type == Item::up ? textDim : accent2.withAlpha (0.85f));
        g.setColour (it.type == Item::up ? textDim : text);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (it.label, 40, 0, w - 100, h, juce::Justification::centredLeft, true);
        if (it.type == Item::folder)
        {
            g.setColour (textDim);
            g.setFont (juce::FontOptions (11.0f));
            g.drawText (juce::String (it.count), w - 60, 0, 40, h, juce::Justification::centredRight);
            g.drawText (">", w - 18, 0, 12, h, juce::Justification::centred);
        }
        return;
    }

    const auto& e = it.entry;
    const bool loaded = e.file == loadedFile();
    if (loaded)
    {
        g.setColour (slot (active()));
        g.fillRoundedRectangle (2.0f, 8.0f, 3.0f, (float) h - 16.0f, 1.5f);
    }
    // Kleine Punkte: in welchen Slots das Sample liegt
    for (int s = 0, dx = 0; s < numSlots; ++s)
        if (processor.getSlotFile (s) == e.file)
        {
            g.setColour (slot (s));
            g.fillEllipse ((float) (w - 80 - dx), (float) h * 0.5f - 3.0f, 6.0f, 6.0f);
            dx += 9;
        }

    g.setColour (selected || loaded ? text : text.withAlpha (0.85f));
    g.setFont (juce::FontOptions (13.0f, loaded ? juce::Font::bold : juce::Font::plain));
    if (search.getText().isNotEmpty())
    {
        g.drawText (e.name, 12, 4, w - 116, h / 2 - 2, juce::Justification::bottomLeft, true);
        g.setColour (textDim);
        g.setFont (juce::FontOptions (10.5f));
        g.drawText (e.folder, 12, h / 2 + 3, w - 76, h / 2 - 5, juce::Justification::topLeft, true);
    }
    else
    {
        g.drawText (e.name, 12, 0, w - 116, h, juce::Justification::centredLeft, true);
    }

    g.setColour (textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText (juce::String (juce::roundToInt (e.seconds * 1000.0)) + " ms", w - 66, 0, 58, h,
                juce::Justification::centredRight);
}

//==============================================================================
void LibraryPanel::rebuild()
{
    items.clear();
    const auto query = search.getText().toLowerCase();

    if (query.isNotEmpty())
    {
        const auto terms = juce::StringArray::fromTokens (query, " ", "");
        for (auto& e : all)
        {
            const auto hay = (e.name + " " + e.folder).toLowerCase();
            bool ok = true;
            for (auto& t : terms)
                if (t.isNotEmpty() && ! hay.contains (t)) { ok = false; break; }
            if (ok)
                items.push_back ({ Item::sample, e.name, e.folder, 0, e });
        }
    }
    else
    {
        if (currentPath.isEmpty())
        {
            // Oberste Ebene: nur die importierten Ordner
            std::map<juce::String, int> roots;
            for (auto& e : all)
                roots[rootOf (e.folder)]++;
            std::vector<std::pair<juce::String, int>> folders (roots.begin(), roots.end());
            std::sort (folders.begin(), folders.end(), [] (const auto& x, const auto& y) { return x.first.compareNatural (y.first) < 0; });
            for (auto& f : folders)
                items.push_back ({ Item::folder, f.first, f.first, f.second, {} });
        }
        else
        {
            // Im Ordner: alle Samples flach, Unterordner aufgeloest
            items.push_back ({ Item::up, "..", {}, 0, {} });
            for (auto& e : siblingsOf (currentPath))
                items.push_back ({ Item::sample, e.name, e.folder, 0, e });
        }
    }

    lastAuditioned = -1;
    suppressAudition = true;
    list.updateContent();
    list.deselectAllRows();

    const auto loaded = loadedFile();
    for (int i = 0; i < (int) items.size(); ++i)
        if (items[(size_t) i].type == Item::sample && items[(size_t) i].entry.file == loaded)
        {
            list.selectRow (i);
            lastAuditioned = i;
            break;
        }
    suppressAudition = false;

    upButton.setEnabled (currentPath.isNotEmpty() && search.getText().isEmpty());
    repaint();
}

void LibraryPanel::openFolder (const juce::String& path)
{
    currentPath = path;
    rebuild();
    list.scrollToEnsureRowIsOnscreen (0);
}

void LibraryPanel::goUp()
{
    if (search.getText().isNotEmpty() || currentPath.isEmpty())
        return;
    const auto from = currentPath;
    openFolder ({});
    for (int i = 0; i < (int) items.size(); ++i)
        if (items[(size_t) i].type == Item::folder && items[(size_t) i].path == from)
        {
            suppressAudition = true;
            list.selectRow (i);
            suppressAudition = false;
            break;
        }
}

void LibraryPanel::selectedRowsChanged (int lastRowSelected)
{
    if (suppressAudition || lastRowSelected < 0 || lastRowSelected >= (int) items.size())
        return;
    if (items[(size_t) lastRowSelected].type == Item::sample)
        audition (lastRowSelected);
}

void LibraryPanel::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    if (row < 0 || row >= (int) items.size())
        return;
    const auto type = items[(size_t) row].type;
    const auto path = items[(size_t) row].path;
    if (type == Item::up)            goUp();
    else if (type == Item::folder)   openFolder (path);
    else if (row == lastAuditioned)  audition (row);   // nochmal abspielen
}

void LibraryPanel::returnKeyPressed (int row)
{
    if (row < 0 || row >= (int) items.size())
        return;
    const auto type = items[(size_t) row].type;
    if (type == Item::up)            goUp();
    else if (type == Item::folder)   openFolder (items[(size_t) row].path);
    else                             audition (row);
}

int LibraryPanel::active() const
{
    return juce::jlimit (0, numSlots - 1, processor.activeSlot.load());
}

juce::File LibraryPanel::loadedFile() const
{
    return processor.getSlotFile (active());
}

void LibraryPanel::activeSlotChanged()
{
    revealLoaded();
    rebuild();
}

bool LibraryPanel::loadAndPlay (int slotIndex, const juce::File& file)
{
    if (file != processor.getSlotFile (slotIndex))
    {
        if (! file.existsAsFile() || ! processor.loadSample (slotIndex, file))
        {
            status.setText ("File not found: " + file.getFileName(), juce::dontSendNotification);
            return false;
        }
    }
    processor.triggerPreview (slotIndex);
    return true;
}

void LibraryPanel::audition (int row)
{
    if (row < 0 || row >= (int) items.size() || items[(size_t) row].type != Item::sample)
        return;
    if (loadAndPlay (active(), items[(size_t) row].entry.file))
    {
        lastAuditioned = row;
        list.repaint();
    }
}

std::vector<SampleLibrary::Entry> LibraryPanel::siblingsOf (const juce::String& root) const
{
    std::vector<SampleLibrary::Entry> result;
    for (auto& e : all)
        if (rootOf (e.folder) == root)
            result.push_back (e);
    std::sort (result.begin(), result.end(), [] (const auto& a, const auto& b)
    {
        const int c = a.name.compareNatural (b.name);
        return c != 0 ? c < 0 : a.folder.compareNatural (b.folder) < 0;
    });
    return result;
}

juce::String LibraryPanel::folderOfLoaded (int slotIndex) const
{
    const auto loaded = processor.getSlotFile (slotIndex);
    for (auto& e : all)
        if (e.file == loaded)
            return e.folder;
    return {};
}

void LibraryPanel::revealLoaded()
{
    const auto loaded = loadedFile();
    for (auto& e : all)
        if (e.file == loaded)
        {
            if (search.getText().isEmpty())
                currentPath = rootOf (e.folder);
            return;
        }
}

void LibraryPanel::step (int slotIndex, int delta)
{
    const auto loaded = processor.getSlotFile (slotIndex);
    if (loaded == juce::File())
        return;

    std::vector<juce::File> siblings;
    const bool inIndex = std::any_of (all.begin(), all.end(), [&] (const auto& e) { return e.file == loaded; });
    const auto folder = folderOfLoaded (slotIndex);

    if (inIndex)
    {
        // Alle One-Shots im selben importierten Ordner (flach)
        for (auto& e : siblingsOf (rootOf (folder)))
            siblings.push_back (e.file);
    }
    else
    {
        // Nicht im Index: Audiodateien im selben Ordner auf der Platte
        for (auto& f : loaded.getParentDirectory().findChildFiles (juce::File::findFiles, false,
                                                                  "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg;*.m4a;*.caf"))
            if (! f.getFileName().startsWithChar ('.'))
                siblings.push_back (f);
        std::sort (siblings.begin(), siblings.end(), [] (const auto& a, const auto& b)
                   { return a.getFileName().compareNatural (b.getFileName()) < 0; });
    }

    if (siblings.size() < 2)
        return;

    int idx = 0;
    for (int i = 0; i < (int) siblings.size(); ++i)
        if (siblings[(size_t) i] == loaded) { idx = i; break; }
    idx = (idx + delta + (int) siblings.size()) % (int) siblings.size();

    if (loadAndPlay (slotIndex, siblings[(size_t) idx]))
    {
        if (inIndex && search.getText().isEmpty())
            currentPath = rootOf (folder);
        rebuild();
        if (lastAuditioned >= 0)
            list.scrollToEnsureRowIsOnscreen (lastAuditioned);
    }
}

void LibraryPanel::chooseFolder()
{
    chooser = std::make_unique<juce::FileChooser> ("Add sample folder");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto dir = fc.getResult();
                              if (dir.isDirectory())
                                  processor.getLibrary().addFolder (dir);
                          });
}

void LibraryPanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    auto entries = processor.getLibrary().getEntries();
    if (entries.size() != all.size())
    {
        all = std::move (entries);
        rebuild();
    }
    refreshStatus();
}

void LibraryPanel::refreshStatus()
{
    auto& lib = processor.getLibrary();
    if (lib.isScanning())
        status.setText ("Scanning " + lib.getCurrentFolderName() + " ... " + juce::String (lib.getFilesChecked())
                            + " files, " + juce::String (lib.getOneShotsFound()) + " one-shots",
                        juce::dontSendNotification);
    else
        status.setText (all.empty() ? juce::String ("No index yet") : juce::String ((int) all.size()) + " one-shots indexed",
                        juce::dontSendNotification);
}
