#include "settings.h"
#include "ui_settings.h"
#include "cr3widget.h"
#include "crqtutil.h"
#include "fallbackfontsdialog.h"
#include <qglobal.h>
#if QT_VERSION >= 0x050000
#include <QtWidgets/QColorDialog>
#include <QtWidgets/QStyleFactory>
#else
#include <QtGui/QColorDialog>
#include <QtGui/QStyleFactory>
#endif
#include <QDir>

#include "lvrend.h"         // for BLOCK_RENDERING_FLAGS_XXX presets

static int def_margins[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 15, 20, 25, 30, 40, 50, 60 };
#define MAX_MARGIN_INDEX (sizeof(def_margins)/sizeof(int))

DECL_DEF_CR_FONT_SIZES;

#if USE_FT_EMBOLDEN
static int synth_weights[] = { 300, 400, 500, 600, 700, 800, 900 };
#else
static int synth_weights[] = { 600 };
#endif
#define SYNTH_WEIGHTS_SZ    (sizeof(synth_weights)/sizeof(int))

static int rend_flags[] = { BLOCK_RENDERING_FLAGS_LEGACY, BLOCK_RENDERING_FLAGS_FLAT, BLOCK_RENDERING_FLAGS_BOOK, BLOCK_RENDERING_FLAGS_WEB };
#define MAX_REND_FLAGS_INDEX (sizeof(rend_flags)/sizeof(int))
static int DOM_versions[] = { 0, gDOMVersionCurrent };
#define MAX_DOM_VERSIONS_INDEX (sizeof(DOM_versions)/sizeof(int))

static bool initDone = false;
static bool suppressOnChange = false;

static void findImagesFromDirectory( lString32 dir, lString32Collection & files ) {
    LVAppendPathDelimiter(dir);
    if ( !LVDirectoryExists(dir) )
        return;
    LVContainerRef cont = LVOpenDirectory(dir.c_str());
    if ( !cont.isNull() ) {
        for ( int i=0; i<cont->GetObjectCount(); i++ ) {
            const LVContainerItemInfo * item  = cont->GetObjectInfo(i);
            if ( !item->IsContainer() ) {
                lString32 name = item->GetName();
                name.lowercase();
                if ( name.endsWith(".png") || name.endsWith(".jpg") || name.endsWith(".gif")
                    || name.endsWith(".jpeg") ) {
                    files.add(dir + item->GetName());
                }
            }
        }
    }
}

static void findBackgrounds( lString32Collection & baseDirs, lString32Collection & files ) {
    int i;
    for ( i=0; i<baseDirs.length(); i++ ) {
        lString32 baseDir = baseDirs[i];
        LVAppendPathDelimiter(baseDir);
        findImagesFromDirectory( baseDir + "backgrounds", files );
    }
    for ( i=0; i<baseDirs.length(); i++ ) {
        lString32 baseDir = baseDirs[i];
        LVAppendPathDelimiter(baseDir);
        findImagesFromDirectory( baseDir + "textures", files );
    }
}

static QString getWeightName(int weight) {
    QString name;
    switch (weight) {
    case 100:
        name = QString("Thin");
        break;
    case 200:
        name = QString("ExtraLight");
        break;
    case 300:
        name = QString("Light");
        break;
    case 350:
        name = QString("Book");
        break;
    case 400:
        name = QString("Regular");
        break;
    case 500:
        name = QString("Medium");
        break;
    case 600:
        name = QString("SemiBold");
        break;
    case 700:
        name = QString("Bold");
        break;
    case 800:
        name = QString("ExtraBold");
        break;
    case 900:
        name = QString("Heavy/Black");
        break;
    case 950:
        name = QString("ExtraBlack");
        break;
    }
    return name;
}

static const char * styleNames[] = {
    "def",
    "title",
    "subtitle",
    "pre",
    "link",
    "cite",
    "epigraph",
    "poem",
    "text-author",
    "footnote-link",
    "footnote",
    "footnote-title",
    "annotation",
    NULL
};

static int interline_spaces[] = {75, 80, 85, 90, 95, 100, 110, 120, 140, 150};

SettingsDlg::SettingsDlg(QWidget *parent, CR3View * docView ) :
    QDialog(parent),
    m_ui(new Ui::SettingsDlg),
    m_docview( docView )
{
    initDone = false;

    m_ui->setupUi(this);
    m_props = m_docview->getOptions();

    m_oldHyph = cr2qt(HyphMan::getSelectedDictionary()->getId());

#ifdef _LINUX
    QString homeDir = QDir::toNativeSeparators(QDir::homePath() + "/.cr3/");
#else
    QString homeDir = QDir::toNativeSeparators(QDir::homePath() + "/cr3/");
#endif
#if MAC==1
    QString exeDir = QDir::toNativeSeparators(qApp->applicationDirPath() + "/../Resources/"); //QDir::separator();
#elif _LINUX
    QString exeDir = QString(CR3_DATA_DIR);
#else
    QString exeDir = QDir::toNativeSeparators(qApp->applicationDirPath() + "/"); //QDir::separator();
#endif

    lString32Collection baseDirs;
    baseDirs.add(qt2cr(homeDir));
    baseDirs.add(qt2cr(exeDir));
#ifdef _LINUX
    baseDirs.add(cs32("/usr/local/share/cr3/"));
#endif
    lString32Collection bgFiles;
    QStringList bgFileLabels;
    findBackgrounds( baseDirs, bgFiles );
    int bgIndex = 0;
    m_backgroundFiles.append("[NONE]");
    bgFileLabels.append("[NONE]");
    QString bgFile = m_props->getStringDef(PROP_BACKGROUND_IMAGE, "");
    for ( int i=0; i<bgFiles.length(); i++ ) {
        lString32 fn = bgFiles[i];
        QString f = cr2qt(fn);
        if ( f==bgFile )
            bgIndex = i + 1;
        m_backgroundFiles.append(f);
        fn = LVExtractFilenameWithoutExtension(fn);
        bgFileLabels.append(cr2qt(fn));
    }
    m_ui->cbPageSkin->clear();
    m_ui->cbPageSkin->addItems( bgFileLabels );
    m_ui->cbPageSkin->setCurrentIndex( bgIndex );

    optionToUi( PROP_WINDOW_FULLSCREEN, m_ui->cbWindowFullscreen );
    optionToUi( PROP_WINDOW_SHOW_MENU, m_ui->cbWindowShowMenu );
    optionToUi( PROP_WINDOW_SHOW_SCROLLBAR, m_ui->cbWindowShowScrollbar );
    optionToUi( PROP_WINDOW_TOOLBAR_SIZE, m_ui->cbWindowShowToolbar );
    optionToUi( PROP_WINDOW_SHOW_STATUSBAR, m_ui->cbWindowShowStatusBar );

    optionToUi( PROP_FOOTNOTES, m_ui->cbShowFootNotes );
    optionToUi( PROP_SHOW_BATTERY, m_ui->cbShowBattery );
    optionToUi( PROP_SHOW_TIME, m_ui->cbShowClock );
    optionToUi( PROP_SHOW_TITLE, m_ui->cbShowBookName );
    optionToUi( PROP_TXT_OPTION_PREFORMATTED, m_ui->cbTxtPreFormatted );
    optionToUi( PROP_EMBEDDED_STYLES, m_ui->cbEnableDocumentStyles );
    optionToUi( PROP_EMBEDDED_FONTS, m_ui->cbEnableEmbeddedFonts );
    m_ui->cbEnableEmbeddedFonts->setEnabled(m_props->getBoolDef(PROP_EMBEDDED_STYLES, true));
    optionToUi( PROP_TXT_OPTION_PREFORMATTED, m_ui->cbTxtPreFormatted );
    optionToUi( PROP_FONT_KERNING_ENABLED, m_ui->cbFontKerning );
    optionToUi( PROP_FLOATING_PUNCTUATION, m_ui->cbFloatingPunctuation );
    optionToUiIndex( PROP_IMG_SCALING_ZOOMIN_INLINE_MODE, m_ui->cbImageInlineZoominMode );
    optionToUiIndex( PROP_IMG_SCALING_ZOOMIN_INLINE_SCALE, m_ui->cbImageInlineZoominScale );
    optionToUiIndex( PROP_IMG_SCALING_ZOOMOUT_INLINE_MODE, m_ui->cbImageInlineZoomoutMode );
    optionToUiIndex( PROP_IMG_SCALING_ZOOMOUT_INLINE_SCALE, m_ui->cbImageInlineZoomoutScale );
    optionToUiIndex( PROP_IMG_SCALING_ZOOMIN_BLOCK_MODE, m_ui->cbImageBlockZoominMode );
    optionToUiIndex( PROP_IMG_SCALING_ZOOMIN_BLOCK_SCALE, m_ui->cbImageBlockZoominScale );
    optionToUiIndex( PROP_IMG_SCALING_ZOOMOUT_BLOCK_MODE, m_ui->cbImageBlockZoomoutMode );
    optionToUiIndex( PROP_IMG_SCALING_ZOOMOUT_BLOCK_SCALE, m_ui->cbImageBlockZoomoutScale );

    QString gamma = m_props->getStringDef(PROP_FONT_GAMMA, "");
    if ( gamma=="" )
        m_props->setString(PROP_FONT_GAMMA, "1.0");
    optionToUiString(PROP_FONT_GAMMA, m_ui->cbFontGamma);

    QString ignoreDocMargins = m_props->getStringDef("styles.body.margin", "");
    m_ui->cbIgnoreDocumentMargins->setCheckState(ignoreDocMargins.length() > 0 ?  Qt::Checked : Qt::Unchecked);

    optionToUiIndex( PROP_STATUS_LINE, m_ui->cbShowPageHeader );

    bool b = m_props->getIntDef( PROP_STATUS_LINE, 0 ) > 0;
    m_ui->cbShowBattery->setEnabled( b );
    m_ui->cbShowClock->setEnabled( b );
    m_ui->cbShowBookName->setEnabled( b );

    m_ui->cbStartupAction->setCurrentIndex( m_props->getIntDef( PROP_APP_START_ACTION, 0 ) );



    int lp = m_props->getIntDef( PROP_LANDSCAPE_PAGES, 2 );
    int vm = m_props->getIntDef( PROP_PAGE_VIEW_MODE, 1 );
    if ( vm==0 )
        m_ui->cbViewMode->setCurrentIndex( 2 );
    else
        m_ui->cbViewMode->setCurrentIndex( lp==1 ? 0 : 1 );
    int hinting = m_props->getIntDef(PROP_FONT_HINTING, 2);
    m_ui->cbFontHinting->setCurrentIndex(hinting);
    int shaping = m_props->getIntDef(PROP_FONT_SHAPING, 1);
    m_ui->cbFontShaping->setCurrentIndex(shaping);
    int highlight = m_props->getIntDef(PROP_HIGHLIGHT_COMMENT_BOOKMARKS, 1);
    m_ui->cbBookmarkHighlightMode->setCurrentIndex(highlight);

    int rendFlagsIndex = 0;
    int rendFlags = m_props->getIntDef(PROP_RENDER_BLOCK_RENDERING_FLAGS, BLOCK_RENDERING_FLAGS_WEB);
    for (int i = 0; i < MAX_REND_FLAGS_INDEX; i++) {
        if (rendFlags == rend_flags[i]) {
            rendFlagsIndex = i;
            break;
        }
    }
    m_ui->cbRendFlags->setCurrentIndex(rendFlagsIndex);

    int DOMVersionIndex = 0;
    int DOMVersion = m_props->getIntDef(PROP_REQUESTED_DOM_VERSION, gDOMVersionCurrent);
    for (int i = 0; i < MAX_DOM_VERSIONS_INDEX; i++) {
        if (DOMVersion == DOM_versions[i]) {
            DOMVersionIndex = i;
            break;
        }
    }
    m_ui->cbDOMLevel->setCurrentIndex(DOMVersionIndex);

    int n = m_props->getIntDef( PROP_PAGE_MARGIN_LEFT, 8 );
    int mi = 0;
    for (int i=0; i <  (int)MAX_MARGIN_INDEX; i++) {
        if (n <= def_margins[i]) {
            mi = i;
            break;
        }
    }
    CRLog::debug("initial margins index: %d", mi);
    m_ui->cbMargins->setCurrentIndex( mi );

    QStringList styles = QStyleFactory::keys();
    QString style = m_props->getStringDef( PROP_WINDOW_STYLE, "" );
    m_ui->cbLookAndFeel->addItems( styles );
    QStyle * s = QApplication::style();
    QString currStyle = s->objectName();
    CRLog::debug("Current system style is %s", currStyle.toUtf8().data() );
    if ( !styles.contains(style, Qt::CaseInsensitive) )
        style = currStyle;
    int index = styles.indexOf( style, Qt::CaseInsensitive );
    if ( index >=0 )
        m_ui->cbLookAndFeel->setCurrentIndex( index );

    crGetFontFaceList( m_faceList );
    m_ui->cbTextFontFace->addItems( m_faceList );
    m_ui->cbTitleFontFace->addItems( m_faceList );
    QStringList sizeList;
    LVArray<int> sizes( cr_font_sizes, sizeof(cr_font_sizes)/sizeof(int) );
    for ( int i=0; i<sizes.length(); i++ )
        sizeList.append( QString("%1").arg(sizes[i]) );
    m_ui->cbTextFontSize->addItems( sizeList );
    m_ui->cbTitleFontSize->addItems( sizeList );
    
    m_defFontFace = "DejaVu Sans";
    static const char * goodFonts[] = {
        "DejaVu Sans",
        "Noto Sans",
        "FreeSans",
        "Liberation Sans",
        "Arial",
        NULL
    };
    for ( int i=0; goodFonts[i]; i++ ) {
        if ( m_faceList.indexOf(QString(goodFonts[i]))>=0 ) {
            m_defFontFace = goodFonts[i];
            break;
        }
    }

    fontToUi( PROP_FONT_FACE, PROP_FONT_SIZE, m_ui->cbTextFontFace, m_ui->cbTextFontSize, m_defFontFace.toLatin1().data() );
    fontToUi( PROP_STATUS_FONT_FACE, PROP_STATUS_FONT_SIZE, m_ui->cbTitleFontFace, m_ui->cbTitleFontSize, m_defFontFace.toLatin1().data() );

    updateFontWeights();

//		{_("90%"), "90"},
//		{_("100%"), "100"},
//		{_("110%"), "110"},
//		{_("120%"), "120"},
//		{_("140%"), "140"},
    //PROP_INTERLINE_SPACE
    //PROP_HYPHENATION_DICT
    QString v = QString("%1").arg(m_props->getIntDef(PROP_INTERLINE_SPACE, 100)) + "%";
    QStringList isitems;
    for (int i = 0; i < (int)(sizeof(interline_spaces) / sizeof(int)); i++)
        isitems.append(QString("%1").arg(interline_spaces[i]) + "%");
    m_ui->cbInterlineSpace->addItems(isitems);
    int isi = m_ui->cbInterlineSpace->findText(v);
    m_ui->cbInterlineSpace->setCurrentIndex(isi>=0 ? isi : 6);

    optionToUi( PROP_TEXTLANG_EMBEDDED_LANGS_ENABLED, m_ui->cbMultiLang );
    optionToUi( PROP_TEXTLANG_HYPHENATION_ENABLED, m_ui->cbEnableHyph );
    int hi = -1;
    v = m_props->getStringDef(PROP_HYPHENATION_DICT,"@algorithm"); //HYPH_DICT_ID_ALGORITHM;
    for ( int i=0; i<HyphMan::getDictList()->length(); i++ ) {
        HyphDictionary * item = HyphMan::getDictList()->get( i );
        if (v == cr2qt(item->getFilename() ) ||  v == cr2qt(item->getId() ))
            hi = i;
        QString s = cr2qt( item->getTitle() );
        if ( item->getType()==HDT_NONE )
            s = tr("[No hyphenation]");
        else if ( item->getType()==HDT_ALGORITHM )
            s = tr("[Algorythmic hyphenation]");
        m_ui->cbHyphenation->addItem( s );
    }
    m_ui->cbHyphenation->setCurrentIndex(hi>=0 ? hi : 1);
    bool legacy_render = (0 == rendFlags) || (0 == DOMVersionIndex);
    m_ui->label_47->setVisible(!legacy_render);
    m_ui->cbMultiLang->setVisible(!legacy_render);
    m_ui->label_48->setVisible(!legacy_render);
    m_ui->cbEnableHyph->setVisible(!legacy_render);
    if (legacy_render) {
        m_ui->label_9->setVisible(true);
        m_ui->cbHyphenation->setVisible(true);
    } else {
        bool embedded_lang = m_props->getBoolDef(PROP_TEXTLANG_EMBEDDED_LANGS_ENABLED, true);
        m_ui->label_9->setVisible(!embedded_lang);
        m_ui->cbHyphenation->setVisible(!embedded_lang);
        m_ui->label_48->setVisible(embedded_lang);
        m_ui->cbEnableHyph->setVisible(embedded_lang);
    }


    m_ui->crSample->setOptions( m_props );
    m_ui->crSample->getDocView()->setShowCover( false );
    m_ui->crSample->getDocView()->setViewMode( DVM_SCROLL, 1 );
    QString testPhrase = tr("The quick brown fox jumps over the lazy dog. ");
    m_ui->crSample->getDocView()->createDefaultDocument(lString32::empty_str, qt2cr(testPhrase+testPhrase+testPhrase));

    updateStyleSample();

    m_styleNames.clear();
    m_styleNames.append(tr("Default paragraph style"));
    m_styleNames.append(tr("Title"));
    m_styleNames.append(tr("Subtitle"));
    m_styleNames.append(tr("Preformatted text"));
    m_styleNames.append(tr("Link"));
    m_styleNames.append(tr("Cite / quotation"));
    m_styleNames.append(tr("Epigraph"));
    m_styleNames.append(tr("Poem"));
    m_styleNames.append(tr("Text author"));
    m_styleNames.append(tr("Footnote link"));
    m_styleNames.append(tr("Footnote"));
    m_styleNames.append(tr("Footnote title"));
    m_styleNames.append(tr("Annotation"));
    m_ui->cbStyleName->clear();
    m_ui->cbStyleName->addItems(m_styleNames);
    m_ui->cbStyleName->setCurrentIndex(0);
    initStyleControls("def");

    initDone = true;

    //m_ui->cbPageSkin->addItem(QString("[None]"), QVariant());
}

SettingsDlg::~SettingsDlg()
{
    delete m_ui;
}

bool SettingsDlg::showDlg(  QWidget * parent, CR3View * docView )
{
    SettingsDlg * dlg = new SettingsDlg( parent, docView );
    dlg->setAttribute(Qt::WA_DeleteOnClose, true);
    dlg->show();
    return true;
}

void SettingsDlg::changeEvent(QEvent *e)
{
    switch (e->type()) {
    case QEvent::LanguageChange:
        m_ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void SettingsDlg::on_buttonBox_rejected()
{
	close();
}

void SettingsDlg::on_cbFontKerning_stateChanged(int s)
{
    setCheck( PROP_FONT_KERNING_ENABLED, s );
    updateStyleSample();
}

void SettingsDlg::on_buttonBox_accepted()
{
    m_docview->setOptions( m_props );
    m_docview->getDocView()->requestRender();
    close();
}

void SettingsDlg::optionToUi( const char * optionName, QCheckBox * cb )
{
    int state = ( m_props->getIntDef( optionName, 1 ) != 0 ) ? 1 : 0;
    CRLog::debug("optionToUI(%s,%d)", optionName, state);
    cb->setCheckState( state ? Qt::Checked : Qt::Unchecked );
}

void SettingsDlg::initStyleControls(const char * styleName) {
    m_styleName = styleName;
    QString prefix = QString("styles.") + styleName + ".";
    bool hideInherited = strcmp(styleName, "def") == 0;
    static const char * alignmentStyles[] = {
        "", // inherited
        "text-align: justify",
        "text-align: left",
        "text-align: center",
        "text-align: right",
        NULL,
    };

    QString alignmentStyleNames[] = {
        tr("-"),
        tr("Justify"),
        tr("Left"),
        tr("Center"),
        tr("Right"),
    };
    m_styleItemAlignment.init(prefix + "align", NULL, alignmentStyles, alignmentStyleNames, hideInherited, m_props, m_ui->cbDefAlignment);

    static const char * indentStyles[] = {
        "", // inherited
        "text-indent: 0em",
        "text-indent: 1.2em",
        "text-indent: 2em",
        "text-indent: -1.2em",
        "text-indent: -2em",
        NULL,
    };

    QString indentStyleNames[] = {
        tr("-"),
        tr("No indent"),
        tr("Small Indent"),
        tr("Big Indent"),
        tr("Small Outdent"),
        tr("Big Outdent"),
    };

    m_styleItemIndent.init(prefix + "text-indent", NULL, indentStyles, indentStyleNames, hideInherited, m_props, m_ui->cbDefFirstLine);

    static const char * marginTopStyles[] = {
        "", // inherited
        "margin-top: 0em",
        "margin-top: 0.2em",
        "margin-top: 0.3em",
        "margin-top: 0.5em",
        "margin-top: 1em",
        "margin-top: 2em",
        NULL,
    };

    static const char * marginBottomStyles[] = {
        "", // inherited
        "margin-bottom: 0em",
        "margin-bottom: 0.2em",
        "margin-bottom: 0.3em",
        "margin-bottom: 0.5em",
        "margin-bottom: 1em",
        "margin-bottom: 2em",
        NULL,
    };

    QString marginTopBottomStyleNames[] = {
        tr("-"),
        tr("0"),
        tr("20% of line height"),
        tr("30% of line height"),
        tr("50% of line height"),
        tr("100% of line height"),
        tr("150% of line height"),
    };

    static const char * marginLeftStyles[] = {
        "", // inherited
        "margin-left: 0em",
        "margin-left: 0.5em",
        "margin-left: 1em",
        "margin-left: 1.5em",
        "margin-left: 2em",
        "margin-left: 4em",
        "margin-left: 10%",
        "margin-left: 15%",
        "margin-left: 20%",
        "margin-left: 30%",
        NULL,
    };

    static const char * marginRightStyles[] = {
        "", // inherited
        "margin-right: 0em",
        "margin-right: 0.5em",
        "margin-right: 1em",
        "margin-right: 1.5em",
        "margin-right: 2em",
        "margin-right: 4em",
        "margin-right: 5%",
        "margin-right: 10%",
        "margin-right: 15%",
        "margin-right: 20%",
        "margin-right: 30%",
        NULL,
    };

    QString marginLeftRightStyleNames[] = {
        tr("-"),
        tr("0"),
        tr("50% of line height"),
        tr("100% of line height"),
        tr("150% of line height"),
        tr("200% of line height"),
        tr("400% of line height"),
        tr("5% of line width"),
        tr("10% of line width"),
        tr("15% of line width"),
        tr("20% of line width"),
        tr("30% of line width"),
    };

    m_styleItemMarginBefore.init(prefix + "margin-top", NULL, marginTopStyles, marginTopBottomStyleNames, hideInherited, m_props, m_ui->cbDefMarginBefore);
    m_styleItemMarginAfter.init(prefix + "margin-bottom", NULL, marginBottomStyles, marginTopBottomStyleNames, hideInherited, m_props, m_ui->cbDefMarginAfter);
    m_styleItemMarginLeft.init(prefix + "margin-left", NULL, marginLeftStyles, marginLeftRightStyleNames, false, m_props, m_ui->cbDefMarginLeft);
    m_styleItemMarginRight.init(prefix + "margin-right", NULL, marginRightStyles, marginLeftRightStyleNames, false, m_props, m_ui->cbDefMarginRight);


    static const char * fontWeightStyles[] = {
        "", // inherited
        "font-weight: normal",
        "font-weight: bold",
        "font-weight: bolder",
        "font-weight: lighter",
        NULL,
    };
    QString fontWeightStyleNames[] = {
        tr("-"),
        tr("Normal"),
        tr("Bold"),
        tr("Bolder"),
        tr("Lighter"),
    };
    m_styleFontWeight.init(prefix + "font-weight", NULL, fontWeightStyles, fontWeightStyleNames, false, m_props, m_ui->cbDefFontWeight);

    static const char * fontSizeStyles[] = {
        "", // inherited
        "font-size: 110%",
        "font-size: 120%",
        "font-size: 150%",
        "font-size: 90%",
        "font-size: 80%",
        "font-size: 70%",
        "font-size: 60%",
        NULL,
    };
    QString fontSizeStyleNames[] = {
        tr("-"),
        tr("Increase: 110%"),
        tr("Increase: 120%"),
        tr("Increase: 150%"),
        tr("Decrease: 90%"),
        tr("Decrease: 80%"),
        tr("Decrease: 70%"),
        tr("Decrease: 60%"),
    };
    m_styleFontSize.init(prefix + "font-size", NULL, fontSizeStyles, fontSizeStyleNames, false, m_props, m_ui->cbDefFontSize);

    static const char * fontStyleStyles[] = {
        "", // inherited
        "font-style: normal",
        "font-style: italic",
        NULL,
    };
    QString fontStyleStyleNames[] = {
        tr("-"),
        tr("Normal"),
        tr("Italic"),
    };
    m_styleFontStyle.init(prefix + "font-style", NULL, fontStyleStyles, fontStyleStyleNames, false, m_props, m_ui->cbDefFontStyle);

    QStringList faces;
    QStringList faceValues;
    faces.append("-");
    faceValues.append("");
    faces.append(tr("[Default Sans Serif]"));
    faceValues.append("font-family: sans-serif");
    faces.append(tr("[Default Serif]"));
    faceValues.append("font-family: serif");
    faces.append(tr("[Default Monospace]"));
    faceValues.append("font-family: \"Courier New\", \"Courier\", monospace");
    for (int i=0; i<m_faceList.length(); i++) {
        QString face = m_faceList.at(i);
        faces.append(face);
        faceValues.append(QString("font-family: " + face));
    }
    m_styleFontFace.init(prefix + "font-face", faceValues, faces, m_props, m_ui->cbDefFontFace);

    static const char * fontColorStyles[] = {
        "", // inherited
        "color: black",
        "color: green",
        "color: silver",
        "color: lime",
        "color: gray",
        "color: olive",
        "color: white",
        "color: yellow",
        "color: maroon",
        "color: navy",
        "color: red",
        "color: blue",
        "color: purple",
        "color: teal",
        "color: fuchsia",
        "color: aqua",
        NULL,
    };
    QString fontColorStyleNames[] = {
        tr("-"),
        tr("Black"),
        tr("Green"),
        tr("Silver"),
        tr("Lime"),
        tr("Gray"),
        tr("Olive"),
        tr("White"),
        tr("Yellow"),
        tr("Maroon"),
        tr("Navy"),
        tr("Red"),
        tr("Blue"),
        tr("Purple"),
        tr("Teal"),
        tr("Fuchsia"),
        tr("Aqua"),
    };
    m_styleFontColor.init(prefix + "color", NULL, fontColorStyles, fontColorStyleNames, false, m_props, m_ui->cbDefFontColor);

    static const char * lineHeightStyles[] = {
        "", // inherited
        "line-height: 75%",
        "line-height: 80%",
        "line-height: 85%",
        "line-height: 90%",
        "line-height: 95%",
        "line-height: 100%",
        "line-height: 110%",
        "line-height: 120%",
        "line-height: 130%",
        "line-height: 140%",
        "line-height: 150%",
        NULL,
    };
    QString lineHeightStyleNames[] = {
        "-",
        "75%",
        "80%",
        "85%",
        "90%",
        "95%",
        "100%",
        "110%",
        "120%",
        "130%",
        "140%",
        "150%",
    };
    m_styleLineHeight.init(prefix + "line-height", NULL, lineHeightStyles, lineHeightStyleNames, false, m_props, m_ui->cbDefLineHeight);

    static const char * textDecorationStyles[] = {
        "", // inherited
        "text-decoration: none",
        "text-decoration: underline",
        "text-decoration: line-through",
        "text-decoration: overline",
        NULL,
    };
    QString textDecorationStyleNames[] = {
        tr("-"),
        tr("None"),
        tr("Underline"),
        tr("Line through"),
        tr("Overline"),
    };
    m_styleTextDecoration.init(prefix + "text-decoration", NULL, textDecorationStyles, textDecorationStyleNames, false, m_props, m_ui->cbDefTextDecoration);

    static const char * verticalAlignStyles[] = {
        "", // inherited
        "vertical-align: baseline",
        "vertical-align: sub",
        "vertical-align: super",
        NULL,
    };
    QString verticalAlignStyleNames[] = {
        tr("-"),
        tr("Baseline"),
        tr("Subscript"),
        tr("Superscript"),
    };
    m_verticalAlignDecoration.init(prefix + "vertical-align", NULL, verticalAlignStyles, verticalAlignStyleNames, false, m_props, m_ui->cbDefVerticalAlign);

}

void SettingsDlg::optionToUiString( const char * optionName, QComboBox * cb )
{
    QString value = m_props->getStringDef( optionName, "" );
    int index = -1;
    for ( int i=0; i<cb->count(); i++ ) {
        if ( cb->itemText(i)==value ) {
            index = i;
            break;
        }
    }
    if ( index<0 )
        index = 0;
    cb->setCurrentIndex( index );
}

void SettingsDlg::optionToUiIndex( const char * optionName, QComboBox * cb )
{
    int value = m_props->getIntDef(optionName, 0);
    if (value < 0)
        value = 0;
    if (value >= cb->count())
        value = cb->count()-1;
    cb->setCurrentIndex(value);
}


void SettingsDlg::optionToUiInversed( const char * optionName, QCheckBox * cb )
{
    int state = ( m_props->getIntDef( optionName, 1 ) != 0 ) ? 1 : 0;
    CRLog::debug("optionToUIInversed(%s,%d)", optionName, state);
    cb->setCheckState( !state ? Qt::Checked : Qt::Unchecked );
}

void SettingsDlg::setCheck( const char * optionName, int checkState )
{
    int value = (checkState == Qt::Checked) ? 1 : 0;
    CRLog::debug("setCheck(%s,%d)", optionName, value);
    m_props->setInt( optionName, value );
}

void SettingsDlg::setCheckInversed( const char * optionName, int checkState )
{
    int value = (checkState == Qt::Checked) ? 0 : 1;
    CRLog::debug("setCheckInversed(%s,%d)", optionName, value);
    m_props->setInt( optionName, value );
}

void SettingsDlg::on_cbWindowFullscreen_stateChanged(int s)
{
    setCheck( PROP_WINDOW_FULLSCREEN, s );
}

void SettingsDlg::on_cbWindowShowToolbar_stateChanged(int s)
{
    setCheck( PROP_WINDOW_TOOLBAR_SIZE, s );
}

void SettingsDlg::on_cbWindowShowMenu_stateChanged(int s)
{
    setCheck( PROP_WINDOW_SHOW_MENU, s );
}

void SettingsDlg::on_cbWindowShowStatusBar_stateChanged(int s)
{
    setCheck( PROP_WINDOW_SHOW_STATUSBAR, s );
}

void SettingsDlg::on_cbWindowShowScrollbar_stateChanged(int s)
{
    setCheck( PROP_WINDOW_SHOW_SCROLLBAR, s );
}

void SettingsDlg::on_cbViewMode_currentIndexChanged(int index)
{
    switch ( index ) {
        case 0:
            m_props->setInt( PROP_LANDSCAPE_PAGES, 1 );
            m_props->setInt( PROP_PAGE_VIEW_MODE, 1 );
            break;
        case 1:
            m_props->setInt( PROP_LANDSCAPE_PAGES, 2 );
            m_props->setInt( PROP_PAGE_VIEW_MODE, 1 );
            break;
        default:
            m_props->setInt( PROP_PAGE_VIEW_MODE, 0 );
            break;
    }
}

void SettingsDlg::on_cbShowPageHeader_currentIndexChanged(int index)
{
    m_props->setInt( PROP_STATUS_LINE, index );
    bool b = index > 0;
    m_ui->cbShowBattery->setEnabled( b );
    m_ui->cbShowClock->setEnabled( b );
    m_ui->cbShowBookName->setEnabled( b );
}

void SettingsDlg::on_cbShowBookName_stateChanged(int s)
{
    setCheck( PROP_SHOW_TITLE, s );
}

void SettingsDlg::on_cbShowClock_stateChanged(int s)
{
    setCheck( PROP_SHOW_TIME, s );
}

void SettingsDlg::on_cbShowBattery_stateChanged(int s)
{
    setCheck( PROP_SHOW_BATTERY, s );
}

void SettingsDlg::on_cbShowFootNotes_stateChanged(int s)
{
    setCheck( PROP_FOOTNOTES, s );

}

void SettingsDlg::on_cbMargins_currentIndexChanged(int index)
{
    int m = def_margins[index];
    CRLog::debug("marginsChanged: %d", index);
    m_props->setInt( PROP_PAGE_MARGIN_BOTTOM, m*2/3 );
    m_props->setInt( PROP_PAGE_MARGIN_TOP, m );
    m_props->setInt( PROP_PAGE_MARGIN_LEFT, m );
    m_props->setInt( PROP_PAGE_MARGIN_RIGHT, m );
}

void SettingsDlg::setBackground( QWidget * wnd, QColor cl )
{
    QPalette pal( wnd->palette() );
    pal.setColor( QPalette::Window, cl );
    wnd->setPalette( pal );
}

void SettingsDlg::updateStyleSample()
{
    QColor txtColor = getColor( PROP_FONT_COLOR, 0x000000 );
    QColor bgColor = getColor( PROP_BACKGROUND_COLOR, 0xFFFFFF );
    QColor headerColor = getColor( PROP_STATUS_FONT_COLOR, 0xFFFFFF );
    QColor selectionColor = getColor( PROP_HIGHLIGHT_SELECTION_COLOR, 0xC0C0C0 );
    QColor commentColor = getColor( PROP_HIGHLIGHT_BOOKMARK_COLOR_COMMENT, 0xC0C000 );
    QColor correctionColor = getColor( PROP_HIGHLIGHT_BOOKMARK_COLOR_CORRECTION, 0xC00000 );
    setBackground( m_ui->frmTextColor, txtColor );
    setBackground( m_ui->frmBgColor, bgColor );
    setBackground( m_ui->frmHeaderTextColor, headerColor );
    setBackground( m_ui->frmSelectionColor, selectionColor );
    setBackground( m_ui->frmCommentColor, commentColor );
    setBackground( m_ui->frmCorrectionColor, correctionColor );

    m_ui->crSample->setOptions( m_props );
    m_ui->crSample->getDocView()->setShowCover( false );
    m_ui->crSample->getDocView()->setViewMode( DVM_SCROLL, 1 );

    m_ui->crSample->getDocView()->getPageImage(0);

    if (!m_props->getBoolDef(PROP_TEXTLANG_EMBEDDED_LANGS_ENABLED, true))
        HyphMan::getDictList()->activate( qt2cr(m_oldHyph) );
}

QColor SettingsDlg::getColor( const char * optionName, unsigned def )
{
    lvColor cr( m_props->getColorDef( optionName, def ) );
    return QColor( cr.r(), cr.g(), cr.b() );
}

void SettingsDlg::setColor( const char * optionName, QColor cl )
{
    m_props->setHex( optionName, lvColor( cl.red(), cl.green(), cl.blue() ).get() );
}

void SettingsDlg::colorDialog( const char * optionName, QString title )
{
    QColorDialog dlg;
    dlg.setWindowTitle(title);
    dlg.setCurrentColor( getColor( optionName, 0x000000 ) );
    if ( dlg.exec() == QDialog::Accepted ) {
        setColor( optionName, dlg.currentColor() );
        updateStyleSample();
    }
}

void SettingsDlg::on_btnTextColor_clicked()
{
    colorDialog( PROP_FONT_COLOR, tr("Text color") );
}

void SettingsDlg::on_btnBgColor_clicked()
{
    colorDialog( PROP_BACKGROUND_COLOR, tr("Background color") );
}

void SettingsDlg::on_btnHeaderTextColor_clicked()
{
    colorDialog( PROP_STATUS_FONT_COLOR, tr("Page header text color") );
}

void SettingsDlg::on_cbLookAndFeel_currentIndexChanged( QString styleName )
{
    if ( !initDone )
        return;
    CRLog::debug( "on_cbLookAndFeel_currentIndexChanged(%s)", styleName.toUtf8().data() );
    m_props->setString( PROP_WINDOW_STYLE, styleName );
}

void SettingsDlg::on_cbTitleFontFace_currentIndexChanged(QString s)
{
    if ( !initDone )
        return;
    m_props->setString( PROP_STATUS_FONT_FACE, s );
}

void SettingsDlg::on_cbTitleFontSize_currentIndexChanged(QString s)
{
    if ( !initDone )
        return;
    m_props->setString( PROP_STATUS_FONT_SIZE, s );
}

void SettingsDlg::on_cbTextFontFace_currentIndexChanged(QString s)
{
    if ( !initDone )
        return;
    m_props->setString( PROP_FONT_FACE, s );
    updateFontWeights();
    updateStyleSample();
}

void SettingsDlg::on_cbTextFontSize_currentIndexChanged(QString s)
{
    if ( !initDone )
        return;
    m_props->setString( PROP_FONT_SIZE, s );
    updateStyleSample();
}

void SettingsDlg::fontToUi( const char * faceOptionName, const char * sizeOptionName, QComboBox * faceCombo, QComboBox * sizeCombo, const char * defFontFace )
{
    QString faceName =  m_props->getStringDef( faceOptionName, defFontFace );
    int faceIndex = faceCombo->findText( faceName );
    if ( faceIndex>=0 )
        faceCombo->setCurrentIndex( faceIndex );
    if (sizeCombo) {
        QString sizeName =  m_props->getStringDef( sizeOptionName, sizeCombo->itemText(4).toUtf8().data() );
        int sizeIndex = sizeCombo->findText( sizeName );
        if ( sizeIndex>=0 )
            sizeCombo->setCurrentIndex( sizeIndex );
    }
}

void SettingsDlg::updateFontWeights() {
    QString selectedFontFace = m_ui->cbTextFontFace->currentText();
    lString8 face8 = UnicodeToUtf8(qt2cr(selectedFontFace));
    int i;
    int weight = 0;
    LVArray<int> weights;
    LVArray<int> nativeWeights;
    fontMan->GetAvailableFontWeights(nativeWeights, face8);
    if (nativeWeights.length() > 0) {
        // combine with synthetic weights
        int synth_idx = 0;
        int j;
        int prev_weight = 0;
        for (i = 0; i < nativeWeights.length(); i++) {
            weight = nativeWeights[i];
            for (j = synth_idx; j < SYNTH_WEIGHTS_SZ; j++) {
                int synth_weight = synth_weights[j];
                if (synth_weight < weight) {
                    if (synth_weight > prev_weight)
                        weights.add(synth_weight);
                }
                else
                    break;
            }
            synth_idx = j;
            weights.add(weight);
            prev_weight = weight;
        }
        for (j = synth_idx; j < SYNTH_WEIGHTS_SZ; j++) {
            if (synth_weights[j] > weight)
                weights.add(synth_weights[j]);
        }
    }
    // fill items
    suppressOnChange = true;
    int normalIndex = 0;
    m_ui->cbFontWeightChange->clear();
    if (weights.empty()) {
        // Invalid font
        return;
    }
    for (i = 0; i < weights.length(); i++) {
        weight = weights[i];
        if (400 == weight)
            normalIndex = i;
        QString label = QString("%1").arg(weight);
        QString descr = getWeightName(weight);
        if (nativeWeights.indexOf(weight) < 0) {
            if (!descr.isEmpty())
                descr += QString(", %1").arg(tr("synthetic*"));
            else
                descr = tr("synthetic");
        }
        if (!descr.isEmpty())
            label = QString("%1 (%2)").arg(label).arg(descr);
        m_ui->cbFontWeightChange->addItem(label, QVariant(weight));
    }
    // select active
    int fontWeightIndex = normalIndex;
    int fontWeight = m_props->getIntDef(PROP_FONT_BASE_WEIGHT, 400);
    for (i = 0; i < weights.length(); i++) {
        if (fontWeight == weights[i]) {
            fontWeightIndex = i;
            break;
        }
    }
    suppressOnChange = false;
    m_ui->cbFontWeightChange->setCurrentIndex(fontWeightIndex);
}

void SettingsDlg::on_cbInterlineSpace_currentIndexChanged(int index)
{
    if ( !initDone )
        return;
    m_props->setInt( PROP_INTERLINE_SPACE, interline_spaces[index] );
    updateStyleSample();
}

void SettingsDlg::on_cbHyphenation_currentIndexChanged(int index)
{
    const QStringList & dl( m_docview->getHyphDicts() );
    QString s = dl[index < dl.count() ? index : 1];
    m_props->setString( PROP_HYPHENATION_DICT, s );
    updateStyleSample();
}

void SettingsDlg::on_cbStartupAction_currentIndexChanged(int index)
{
    m_props->setInt( PROP_APP_START_ACTION, index );
}

void SettingsDlg::on_cbTxtPreFormatted_toggled(bool checked)
{

}

void SettingsDlg::on_cbTxtPreFormatted_stateChanged(int s)
{
    setCheck( PROP_TXT_OPTION_PREFORMATTED, s );
}

void SettingsDlg::on_cbPageSkin_currentIndexChanged(int index)
{
    if ( !initDone )
        return;
    if ( index>=0 && index<m_backgroundFiles.length() )
        m_props->setString( PROP_BACKGROUND_IMAGE, m_backgroundFiles[index] );
}


void SettingsDlg::on_cbFloatingPunctuation_stateChanged(int s)
{
    setCheck( PROP_FLOATING_PUNCTUATION, s );
    updateStyleSample();
}

void SettingsDlg::on_cbFontGamma_currentIndexChanged(QString s)
{
    m_props->setString( PROP_FONT_GAMMA, s );
    updateStyleSample();
}

void SettingsDlg::on_cbStyleName_currentIndexChanged(int index)
{
    if (index >= 0 && initDone)
        initStyleControls(styleNames[index]);
}

void SettingsDlg::on_cbDefAlignment_currentIndexChanged(int index)
{
    m_styleItemAlignment.update(index);
}

void SettingsDlg::on_cbDefFirstLine_currentIndexChanged(int index)
{
    m_styleItemIndent.update(index);
}

void SettingsDlg::on_cbDefMarginBefore_currentIndexChanged(int index)
{
    m_styleItemMarginBefore.update(index);
}

void SettingsDlg::on_cbDefMarginAfter_currentIndexChanged(int index)
{
    m_styleItemMarginAfter.update(index);
}

void SettingsDlg::on_cbDefMarginLeft_currentIndexChanged(int index)
{
    m_styleItemMarginLeft.update(index);
}

void SettingsDlg::on_cbDefMarginRight_currentIndexChanged(int index)
{
    m_styleItemMarginRight.update(index);
}

void SettingsDlg::on_cbDefFontSize_currentIndexChanged(int index)
{
    m_styleFontSize.update(index);
}

void SettingsDlg::on_cbDefFontFace_currentIndexChanged(int index)
{
    m_styleFontFace.update(index);
}

void SettingsDlg::on_cbDefFontWeight_currentIndexChanged(int index)
{
    m_styleFontWeight.update(index);
}

void SettingsDlg::on_cbDefFontStyle_currentIndexChanged(int index)
{
    m_styleFontStyle.update(index);
}

void SettingsDlg::on_cbDefFontColor_currentIndexChanged(int index)
{
    m_styleFontColor.update(index);
}

void SettingsDlg::on_cbFontHinting_currentIndexChanged(int index)
{
    m_props->setInt(PROP_FONT_HINTING, index);
    updateStyleSample();
}

void SettingsDlg::on_cbEnableEmbeddedFonts_toggled(bool checked)
{
    setCheck(PROP_EMBEDDED_FONTS, checked ? Qt::Checked : Qt::Unchecked);
}

void SettingsDlg::on_cbIgnoreDocumentMargins_stateChanged(int s)
{
    QString value = (s == Qt::Checked) ? "margin: 0em !important" : "";
    m_props->setString("styles.body.margin", value);
}

void SettingsDlg::on_cbEnableDocumentStyles_toggled(bool checked)
{
    setCheck(PROP_EMBEDDED_STYLES, checked ? Qt::Checked : Qt::Unchecked);
    m_ui->cbEnableEmbeddedFonts->setEnabled(checked);
    m_ui->cbIgnoreDocumentMargins->setEnabled(checked);
}

void SettingsDlg::on_btnSelectionColor_clicked()
{
    colorDialog( PROP_HIGHLIGHT_SELECTION_COLOR, tr("Selection color") );
}

void SettingsDlg::on_btnCommentColor_clicked()
{
    colorDialog( PROP_HIGHLIGHT_BOOKMARK_COLOR_COMMENT, tr("Comment bookmark color") );
}

void SettingsDlg::on_btnCorrectionColor_clicked()
{
    colorDialog( PROP_HIGHLIGHT_BOOKMARK_COLOR_CORRECTION, tr("Correction bookmark color") );
}

void SettingsDlg::on_cbBookmarkHighlightMode_currentIndexChanged(int index)
{
    if ( !initDone )
        return;
    m_props->setInt(PROP_HIGHLIGHT_COMMENT_BOOKMARKS, index);
}

void SettingsDlg::on_cbImageInlineZoominMode_currentIndexChanged(int index)
{
    m_props->setInt(PROP_IMG_SCALING_ZOOMIN_INLINE_MODE, index);
}

void SettingsDlg::on_cbImageInlineZoominScale_currentIndexChanged(int index)
{
    m_props->setInt(PROP_IMG_SCALING_ZOOMIN_INLINE_SCALE, index);
}

void SettingsDlg::on_cbImageInlineZoomoutMode_currentIndexChanged(int index)
{
    m_props->setInt(PROP_IMG_SCALING_ZOOMOUT_INLINE_MODE, index);
}

void SettingsDlg::on_cbImageInlineZoomoutScale_currentIndexChanged(int index)
{
    m_props->setInt(PROP_IMG_SCALING_ZOOMOUT_INLINE_SCALE, index);
}

void SettingsDlg::on_cbImageBlockZoominMode_currentIndexChanged(int index)
{
    m_props->setInt(PROP_IMG_SCALING_ZOOMIN_BLOCK_MODE, index);
}

void SettingsDlg::on_cbImageBlockZoominScale_currentIndexChanged(int index)
{
    m_props->setInt(PROP_IMG_SCALING_ZOOMIN_BLOCK_SCALE, index);
}

void SettingsDlg::on_cbImageBlockZoomoutMode_currentIndexChanged(int index)
{
    m_props->setInt(PROP_IMG_SCALING_ZOOMOUT_BLOCK_MODE, index);
}

void SettingsDlg::on_cbImageBlockZoomoutScale_currentIndexChanged(int index)
{
    m_props->setInt(PROP_IMG_SCALING_ZOOMOUT_BLOCK_SCALE, index);
}

void SettingsDlg::on_cbFontShaping_currentIndexChanged(int index)
{
    m_props->setInt(PROP_FONT_SHAPING, index);
    updateStyleSample();
}

void SettingsDlg::on_cbRendFlags_currentIndexChanged(int index)
{
    if (index < 0 || index >= MAX_REND_FLAGS_INDEX)
        index = 0;
    m_props->setInt(PROP_RENDER_BLOCK_RENDERING_FLAGS, rend_flags[index]);
    int DOMVersion = m_props->getIntDef(PROP_REQUESTED_DOM_VERSION, gDOMVersionCurrent);
    bool legacy_render = (0 == index) || (DOMVersion < 20180524);
    m_ui->label_47->setVisible(!legacy_render);
    m_ui->cbMultiLang->setVisible(!legacy_render);
    m_ui->label_48->setVisible(!legacy_render);
    m_ui->cbEnableHyph->setVisible(!legacy_render);
    if (legacy_render) {
        m_ui->label_9->setVisible(true);
        m_ui->cbHyphenation->setVisible(true);
    } else {
        bool embedded_lang = m_props->getBoolDef(PROP_TEXTLANG_EMBEDDED_LANGS_ENABLED, true);
        m_ui->label_9->setVisible(!embedded_lang);
        m_ui->cbHyphenation->setVisible(!embedded_lang);
        m_ui->label_48->setVisible(embedded_lang);
        m_ui->cbEnableHyph->setVisible(embedded_lang);
    }
    updateStyleSample();
}

void SettingsDlg::on_cbDOMLevel_currentIndexChanged(int index)
{
    if (index < 0 || index >= MAX_DOM_VERSIONS_INDEX)
        index = 0;
    int rendFlags = m_props->getIntDef(PROP_RENDER_BLOCK_RENDERING_FLAGS, 0);
    bool legacy_render = (0 == rendFlags) || (0 == index);
    m_ui->label_47->setVisible(!legacy_render);
    m_ui->cbMultiLang->setVisible(!legacy_render);
    m_ui->label_48->setVisible(!legacy_render);
    m_ui->cbEnableHyph->setVisible(!legacy_render);
    if (legacy_render) {
        m_ui->label_9->setVisible(true);
        m_ui->cbHyphenation->setVisible(true);
    } else {
        bool embedded_lang = m_props->getBoolDef(PROP_TEXTLANG_EMBEDDED_LANGS_ENABLED, true);
        m_ui->label_9->setVisible(!embedded_lang);
        m_ui->cbHyphenation->setVisible(!embedded_lang);
        m_ui->label_48->setVisible(embedded_lang);
        m_ui->cbEnableHyph->setVisible(embedded_lang);
    }
    m_props->setInt(PROP_REQUESTED_DOM_VERSION, DOM_versions[index]);
    updateStyleSample();
}

void SettingsDlg::on_cbMultiLang_stateChanged(int state)
{
    setCheck( PROP_TEXTLANG_EMBEDDED_LANGS_ENABLED, state );
    m_ui->label_9->setVisible(state != Qt::Checked);
    m_ui->cbHyphenation->setVisible(state != Qt::Checked);
    m_ui->label_48->setVisible(state == Qt::Checked);
    m_ui->cbEnableHyph->setVisible(state == Qt::Checked);
    // don't update preview to not change static field TextLangMan::_embedded_langs_enabled too early!
}

void SettingsDlg::on_cbEnableHyph_stateChanged(int state)
{
    setCheck( PROP_TEXTLANG_HYPHENATION_ENABLED, state );
    // don't update preview to not change static field TextLangMan::_hyphenation_enabled too early!
}

void SettingsDlg::on_btnFallbackMan_clicked()
{
    FallbackFontsDialog dlg(this, m_faceList);
    QString fallbackFaces = m_props->getStringDef( PROP_FALLBACK_FONT_FACES, m_defFontFace.toLatin1().data() );
    dlg.setFallbackFaces(fallbackFaces);
    if ( dlg.exec() == QDialog::Accepted ) {
        fallbackFaces = dlg.fallbackFaces();
        m_props->setString( PROP_FALLBACK_FONT_FACES, fallbackFaces );
        updateStyleSample();
    }
}

void SettingsDlg::on_cbFontWeightChange_currentIndexChanged(int index)
{
    if (suppressOnChange)
        return;
    QVariant data = m_ui->cbFontWeightChange->currentData(Qt::UserRole);
    int weight = data.toInt();
    m_props->setInt(PROP_FONT_BASE_WEIGHT, weight);
    QString face = m_props->getStringDef( PROP_FONT_FACE, "" );
    LVArray<int> nativeWeights;
    fontMan->GetAvailableFontWeights(nativeWeights, UnicodeToUtf8(qt2cr(face)));
    //m_ui->cbFontHinting->setEnabled(nativeWeights.indexOf(weight) >= 0);
    updateStyleSample();
}
