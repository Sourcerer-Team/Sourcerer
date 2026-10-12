#include "QScintillaLexerFactory.h"
#include <QDebug>
#include <QFileInfo>
#include <QSet>
#include <Qsci/qscilexer.h>
#include <Qsci/qscilexerbash.h>
#include <Qsci/qscilexercsharp.h>
#include <Qsci/qscilexerdart.h>
#include <Qsci/qscilexerfsharp.h>
#include <Qsci/qscilexerscala.h>
#include <Qsci/qscilexerhaskell.h>
#include <Qsci/qscilexerjava.h>
#include <Qsci/qscilexerocaml.h>
#include <Qsci/qscilexerocamltype.h>
#include <Qsci/qscilexerpowershell.h>
#include <Qsci/qscilexerrust.h>
#include <Qsci/qscilexertsx.h>
#include <Qsci/qscilexertypescript.h>

#include <Qsci/qscilexerbatch.h>
#include <Qsci/qscilexercmake.h>
#include <Qsci/qscilexercpp.h>
#include <Qsci/qscilexercss.h>
#include <Qsci/qscilexercustom.h>
#include <Qsci/qscilexerd.h>
#include <Qsci/qscilexerdiff.h>
#include <Qsci/qscilexerfortran.h>
#include <Qsci/qscilexerfortran77.h>
#include <Qsci/qscilexerhtml.h>
#include <Qsci/qscilexeridl.h>
#include <Qsci/qscilexerjavascript.h>
#include <Qsci/qscilexerjson.h>
#include <Qsci/qscilexerlua.h>
#include <Qsci/qscilexermakefile.h>
#include <Qsci/qscilexermarkdown.h>
#include <Qsci/qscilexerasm.h>
#include <Qsci/qscilexermasm.h>
#include <Qsci/qscilexermatlab.h>
#include <Qsci/qscilexernasm.h>
#include <Qsci/qscilexeroctave.h>
#include <Qsci/qscilexerpascal.h>
#include <Qsci/qscilexerperl.h>
#include <Qsci/qscilexerpostscript.h>
#include <Qsci/qscilexerpov.h>
#include <Qsci/qscilexerproperties.h>
#include <Qsci/qscilexerpython.h>
#include <Qsci/qscilexergo.h>
#include <Qsci/qscilexerkotlin.h>
#include <Qsci/qscilexerobjectivec.h>
#include <Qsci/qscilexerswift.h>
#include <Qsci/qscilexerzig.h>
#include <Qsci/qscilexerruby.h>
#include <Qsci/qscilexerspice.h>
#include <Qsci/qscilexersql.h>
#include <Qsci/qscilexertcl.h>
#include <Qsci/qscilexertex.h>
#include <Qsci/qscilexerverilog.h>
#include <Qsci/qscilexervhdl.h>
#include <Qsci/qscilexerxml.h>
#include <Qsci/qscilexeryaml.h>

using namespace GuiManager;

namespace {

template <typename Lexer> QsciLexer *createLexer(QObject *parent) {
  return new Lexer(parent);
}

} // namespace

QScintillaLexerFactory::QScintillaLexerFactory(QObject *parent)
    : QObject(parent) {
  initializeExtensionMappings();
  initializeFilenameMappings();
  initializeLexerDescriptions();
}

QScintillaLexerFactory::~QScintillaLexerFactory() {}

QsciLexer *QScintillaLexerFactory::createLexerForFile(const QString &filePath,
                                                      QObject *parent) const {
  const QString normalizedFileName =
      normalizeFileName(QFileInfo(filePath).fileName());

  if (_filenameToLexer.contains(normalizedFileName)) {
    QsciLexer *lexer = createLexerByName(_filenameToLexer.value(normalizedFileName), parent);
    if (lexer) {
      emit lexerCreated(lexer, normalizedFileName);
    }
    return lexer;
  }

  const QFileInfo fileInfo(filePath);
  const QString completeSuffix = normalizeExtension(fileInfo.completeSuffix());
  if (!completeSuffix.isEmpty() && _extensionToLexer.contains(completeSuffix)) {
    QsciLexer *lexer = createLexerByName(_extensionToLexer.value(completeSuffix), parent);
    if (lexer) {
      emit lexerCreated(lexer, completeSuffix);
    }
    return lexer;
  }

  QString extension = extractExtension(filePath);
  return createLexerForExtension(extension, parent);
}

QsciLexer *
QScintillaLexerFactory::createLexerForExtension(const QString &extension,
                                                QObject *parent) const {
  QString normalizedExt = normalizeExtension(extension);

  if (!_extensionToLexer.contains(normalizedExt)) {
    qDebug() << "QScintillaLexerFactory: No lexer found for extension:"
             << normalizedExt;
    return nullptr;
  }

  QString lexerName = _extensionToLexer.value(normalizedExt);
  QsciLexer *lexer = createLexerByName(lexerName, parent);

  if (lexer) {
    emit lexerCreated(lexer, normalizedExt);
  }

  return lexer;
}

QsciLexer *QScintillaLexerFactory::createLexerByName(const QString &lexerName,
                                                     QObject *parent) const {
  return createLexerByType(lexerName, parent);
}

QsciLexer *
QScintillaLexerFactory::createDiffViewerLexerForFile(const QString &filePath,
                                                     QObject *parent) {
  using Creator = QsciLexer *(*)(QObject *);
  // Only extensions whose content the tree-sitter grammar parses correctly
  // (e.g. not scss/less with the CSS grammar).
  static const QHash<QString, Creator> creators = {
      {QStringLiteral("c"), &createLexer<QsciLexerCPP>},
      {QStringLiteral("cc"), &createLexer<QsciLexerCPP>},
      {QStringLiteral("cpp"), &createLexer<QsciLexerCPP>},
      {QStringLiteral("cxx"), &createLexer<QsciLexerCPP>},
      {QStringLiteral("c++"), &createLexer<QsciLexerCPP>},
      {QStringLiteral("h"), &createLexer<QsciLexerCPP>},
      {QStringLiteral("hh"), &createLexer<QsciLexerCPP>},
      {QStringLiteral("hpp"), &createLexer<QsciLexerCPP>},
      {QStringLiteral("hxx"), &createLexer<QsciLexerCPP>},
      {QStringLiteral("h++"), &createLexer<QsciLexerCPP>},
      {QStringLiteral("dart"), &createLexer<QsciLexerDart>},
      {QStringLiteral("htm"), &createLexer<QsciLexerHTML>},
      {QStringLiteral("html"), &createLexer<QsciLexerHTML>},
      {QStringLiteral("xhtml"), &createLexer<QsciLexerHTML>},
      {QStringLiteral("shtml"), &createLexer<QsciLexerHTML>},
      {QStringLiteral("css"), &createLexer<QsciLexerCSS>},
      {QStringLiteral("js"), &createLexer<QsciLexerJavaScript>},
      {QStringLiteral("jsx"), &createLexer<QsciLexerJavaScript>},
      {QStringLiteral("mjs"), &createLexer<QsciLexerJavaScript>},
      {QStringLiteral("ts"), &createLexer<QsciLexerTypeScript>},
      {QStringLiteral("tsx"), &createLexer<QsciLexerTSX>},
      {QStringLiteral("java"), &createLexer<QsciLexerJava>},
      {QStringLiteral("cs"), &createLexer<QsciLexerCSharp>},
      {QStringLiteral("csx"), &createLexer<QsciLexerCSharp>},
      {QStringLiteral("rs"), &createLexer<QsciLexerRust>},
      {QStringLiteral("ml"), &createLexer<QsciLexerOCaml>},
      {QStringLiteral("mli"), &createLexer<QsciLexerOCamlInterface>},
      {QStringLiteral("mlt"), &createLexer<QsciLexerOCamlType>},
      {QStringLiteral("hs"), &createLexer<QsciLexerHaskell>},
      {QStringLiteral("ps1"), &createLexer<QsciLexerPowerShell>},
      {QStringLiteral("psm1"), &createLexer<QsciLexerPowerShell>},
      {QStringLiteral("psd1"), &createLexer<QsciLexerPowerShell>},
      {QStringLiteral("py"), &createLexer<QsciLexerPython>},
      {QStringLiteral("pyi"), &createLexer<QsciLexerPython>},
      {QStringLiteral("pyw"), &createLexer<QsciLexerPython>},
      {QStringLiteral("sh"), &createLexer<QsciLexerBash>},
      {QStringLiteral("bash"), &createLexer<QsciLexerBash>},
      {QStringLiteral("zsh"), &createLexer<QsciLexerBash>},
      {QStringLiteral("bat"), &createLexer<QsciLexerBatch>},
      {QStringLiteral("cmd"), &createLexer<QsciLexerBatch>},
      {QStringLiteral("xml"), &createLexer<QsciLexerXML>},
      {QStringLiteral("xsl"), &createLexer<QsciLexerXML>},
      {QStringLiteral("xslt"), &createLexer<QsciLexerXML>},
      {QStringLiteral("xsd"), &createLexer<QsciLexerXML>},
      {QStringLiteral("svg"), &createLexer<QsciLexerXML>},
      {QStringLiteral("xaml"), &createLexer<QsciLexerXML>},
      {QStringLiteral("fs"), &createLexer<QsciLexerFSharp>},
      {QStringLiteral("fsi"), &createLexer<QsciLexerFSharp>},
      {QStringLiteral("fsx"), &createLexer<QsciLexerFSharp>},
      {QStringLiteral("fsscript"), &createLexer<QsciLexerFSharp>},
      {QStringLiteral("lua"), &createLexer<QsciLexerLua>},
      {QStringLiteral("scala"), &createLexer<QsciLexerScala>},
      {QStringLiteral("sc"), &createLexer<QsciLexerScala>},
      {QStringLiteral("mk"), &createLexer<QsciLexerMakefile>},
      {QStringLiteral("mak"), &createLexer<QsciLexerMakefile>},
      {QStringLiteral("rb"), &createLexer<QsciLexerRuby>},
      {QStringLiteral("rbw"), &createLexer<QsciLexerRuby>},
      {QStringLiteral("go"), &createLexer<QsciLexerGo>},
      {QStringLiteral("zig"), &createLexer<QsciLexerZig>},
      {QStringLiteral("asm"), &createLexer<QsciLexerAsm>},
      {QStringLiteral("s"), &createLexer<QsciLexerAsm>},
      {QStringLiteral("kt"), &createLexer<QsciLexerKotlin>},
      {QStringLiteral("kts"), &createLexer<QsciLexerKotlin>},
      {QStringLiteral("mm"), &createLexer<QsciLexerObjectiveC>},
      {QStringLiteral("swift"), &createLexer<QsciLexerSwift>},
      {QStringLiteral("ruby"), &createLexer<QsciLexerRuby>},
      {QStringLiteral("json"), &createLexer<QsciLexerJSON>},
      {QStringLiteral("jsonc"), &createLexer<QsciLexerJSON>}};

  // Some files are known by their name, without an extension.
  const QString fileName = normalizeFileName(QFileInfo(filePath).fileName());
  if (fileName == QStringLiteral("makefile") ||
      fileName == QStringLiteral("gnumakefile")) {
    return createLexer<QsciLexerMakefile>(parent);
  }
  if (fileName == QStringLiteral("gemfile") ||
      fileName == QStringLiteral("rakefile")) {
    return createLexer<QsciLexerRuby>(parent);
  }

  const Creator creator = creators.value(extractExtension(filePath), nullptr);
  return creator ? creator(parent) : nullptr;
}

QStringList QScintillaLexerFactory::supportedExtensions() const {
  return _extensionToLexer.keys();
}

QStringList QScintillaLexerFactory::supportedLexerNames() const {
  QStringList names = _extensionToLexer.values();
  names.removeDuplicates();
  names.sort();
  return names;
}

QString QScintillaLexerFactory::getLexerNameForExtension(
    const QString &extension) const {
  QString normalizedExt = normalizeExtension(extension);
  return _extensionToLexer.value(normalizedExt, QString());
}

QString QScintillaLexerFactory::getDescriptionForExtension(
    const QString &extension) const {
  QString lexerName = getLexerNameForExtension(extension);
  return _lexerDescriptions.value(lexerName, QString());
}

bool QScintillaLexerFactory::isExtensionSupported(
    const QString &extension) const {
  QString normalizedExt = normalizeExtension(extension);
  return _extensionToLexer.contains(normalizedExt);
}

void QScintillaLexerFactory::addExtensionMapping(const QString &extension,
                                                 const QString &lexerName) {
  QString normalizedExt = normalizeExtension(extension);
  _extensionToLexer.insert(normalizedExt, lexerName);
  emit mappingChanged(normalizedExt, lexerName);
}

void QScintillaLexerFactory::removeExtensionMapping(const QString &extension) {
  QString normalizedExt = normalizeExtension(extension);
  if (_extensionToLexer.remove(normalizedExt)) {
    emit mappingChanged(normalizedExt, QString());
  }
}

void QScintillaLexerFactory::setExtensionMapping(const QString &extension,
                                                 const QString &lexerName) {
  QString normalizedExt = normalizeExtension(extension);
  _extensionToLexer[normalizedExt] = lexerName;
  emit mappingChanged(normalizedExt, lexerName);
}

QString QScintillaLexerFactory::extractExtension(const QString &filePath) {
  QFileInfo fileInfo(filePath);
  return fileInfo.suffix().toLower();
}

QString QScintillaLexerFactory::normalizeExtension(const QString &extension) {
  QString normalized = extension.toLower();
  if (normalized.startsWith('.')) {
    normalized = normalized.mid(1);
  }
  return normalized;
}

QString QScintillaLexerFactory::normalizeFileName(const QString &fileName) {
  QString normalized = fileName.toLower();
  if (normalized.startsWith('.')) {
    normalized = normalized.mid(1);
  }
  return normalized;
}

void QScintillaLexerFactory::refreshMappings() {
  initializeExtensionMappings();
  initializeFilenameMappings();
  initializeLexerDescriptions();
}

// Lexer creation methods
QsciLexer *QScintillaLexerFactory::createCppLexer(QObject *parent) const {
  return new QsciLexerCPP(parent);
}

QsciLexer *QScintillaLexerFactory::createPythonLexer(QObject *parent) const {
  return new QsciLexerPython(parent);
}

QsciLexer *QScintillaLexerFactory::createJavaLexer(QObject *parent) const {
  return new QsciLexerJava(parent);
}

QsciLexer *QScintillaLexerFactory::createTypeScriptLexer(QObject *parent) const {
  return new QsciLexerTypeScript(parent);
}

QsciLexer *QScintillaLexerFactory::createTSXLexer(QObject *parent) const {
  return new QsciLexerTSX(parent);
}

QsciLexer *QScintillaLexerFactory::createCSharpLexer(QObject *parent) const {
  return new QsciLexerCSharp(parent);
}

QsciLexer *QScintillaLexerFactory::createDartLexer(QObject *parent) const {
  return new QsciLexerDart(parent);
}

QsciLexer *QScintillaLexerFactory::createRustLexer(QObject *parent) const {
  return new QsciLexerRust(parent);
}

QsciLexer *QScintillaLexerFactory::createOCamlLexer(QObject *parent) const {
  return new QsciLexerOCaml(parent);
}

QsciLexer *QScintillaLexerFactory::createOCamlInterfaceLexer(QObject *parent) const {
  return new QsciLexerOCamlInterface(parent);
}

QsciLexer *QScintillaLexerFactory::createOCamlTypeLexer(QObject *parent) const {
  return new QsciLexerOCamlType(parent);
}

QsciLexer *QScintillaLexerFactory::createHaskellLexer(QObject *parent) const {
  return new QsciLexerHaskell(parent);
}

QsciLexer *QScintillaLexerFactory::createFSharpLexer(QObject *parent) const {
  return new QsciLexerFSharp(parent);
}

QsciLexer *QScintillaLexerFactory::createScalaLexer(QObject *parent) const {
  return new QsciLexerScala(parent);
}

QsciLexer *QScintillaLexerFactory::createPowerShellLexer(QObject *parent) const {
  return new QsciLexerPowerShell(parent);
}

QsciLexer *QScintillaLexerFactory::createJavaScriptLexer(QObject *parent) const {
  return new QsciLexerJavaScript(parent);
}

QsciLexer *QScintillaLexerFactory::createHtmlLexer(QObject *parent) const {
  return new QsciLexerHTML(parent);
}

QsciLexer *QScintillaLexerFactory::createCssLexer(QObject *parent) const {
  return new QsciLexerCSS(parent);
}

QsciLexer *QScintillaLexerFactory::createXmlLexer(QObject *parent) const {
  return new QsciLexerXML(parent);
}

QsciLexer *QScintillaLexerFactory::createJsonLexer(QObject *parent) const {
  return new QsciLexerJSON(parent);
}

QsciLexer *QScintillaLexerFactory::createSqlLexer(QObject *parent) const {
  return new QsciLexerSQL(parent);
}

QsciLexer *QScintillaLexerFactory::createBashLexer(QObject *parent) const {
  return new QsciLexerBash(parent);
}


QsciLexer *QScintillaLexerFactory::createBatchLexer(QObject *parent) const {
  return new QsciLexerBatch(parent);
}

QsciLexer *QScintillaLexerFactory::createMakefileLexer(QObject *parent) const {
  return new QsciLexerMakefile(parent);
}

QsciLexer *QScintillaLexerFactory::createDiffLexer(QObject *parent) const {
  return new QsciLexerDiff(parent);
}

QsciLexer *
QScintillaLexerFactory::createPropertiesLexer(QObject *parent) const {
  return new QsciLexerProperties(parent);
}

QsciLexer *QScintillaLexerFactory::createYamlLexer(QObject *parent) const {
  return new QsciLexerYAML(parent);
}

QsciLexer *QScintillaLexerFactory::createMarkdownLexer(QObject *parent) const {
  return new QsciLexerMarkdown(parent);
}

QsciLexer *QScintillaLexerFactory::createTexLexer(QObject *parent) const {
  return new QsciLexerTeX(parent);
}

QsciLexer *
QScintillaLexerFactory::createPostScriptLexer(QObject *parent) const {
  return new QsciLexerPostScript(parent);
}

QsciLexer *QScintillaLexerFactory::createMatlabLexer(QObject *parent) const {
  return new QsciLexerMatlab(parent);
}

QsciLexer *QScintillaLexerFactory::createOctaveLexer(QObject *parent) const {
  return new QsciLexerOctave(parent);
}

QsciLexer *QScintillaLexerFactory::createPerlLexer(QObject *parent) const {
  return new QsciLexerPerl(parent);
}

QsciLexer *QScintillaLexerFactory::createGoLexer(QObject *parent) const {
  return new QsciLexerGo(parent);
}

QsciLexer *QScintillaLexerFactory::createZigLexer(QObject *parent) const {
  return new QsciLexerZig(parent);
}

QsciLexer *QScintillaLexerFactory::createObjectiveCLexer(QObject *parent) const {
  return new QsciLexerObjectiveC(parent);
}

QsciLexer *QScintillaLexerFactory::createSwiftLexer(QObject *parent) const {
  return new QsciLexerSwift(parent);
}

QsciLexer *QScintillaLexerFactory::createKotlinLexer(QObject *parent) const {
  return new QsciLexerKotlin(parent);
}

QsciLexer *QScintillaLexerFactory::createRubyLexer(QObject *parent) const {
  return new QsciLexerRuby(parent);
}

QsciLexer *QScintillaLexerFactory::createFortranLexer(QObject *parent) const {
  return new QsciLexerFortran(parent);
}

QsciLexer *QScintillaLexerFactory::createFortran77Lexer(QObject *parent) const {
  return new QsciLexerFortran77(parent);
}

QsciLexer *QScintillaLexerFactory::createIdlLexer(QObject *parent) const {
  return new QsciLexerIDL(parent);
}

QsciLexer *QScintillaLexerFactory::createVhdlLexer(QObject *parent) const {
  return new QsciLexerVHDL(parent);
}

QsciLexer *QScintillaLexerFactory::createVerilogLexer(QObject *parent) const {
  return new QsciLexerVerilog(parent);
}

QsciLexer *QScintillaLexerFactory::createTclLexer(QObject *parent) const {
  return new QsciLexerTCL(parent);
}

QsciLexer *QScintillaLexerFactory::createLuaLexer(QObject *parent) const {
  return new QsciLexerLua(parent);
}

QsciLexer *QScintillaLexerFactory::createPascalLexer(QObject *parent) const {
  return new QsciLexerPascal(parent);
}

QsciLexer *QScintillaLexerFactory::createAsmLexer(QObject *parent) const {
  return new QsciLexerAsm(parent);
}

QsciLexer *QScintillaLexerFactory::createMAsmLexer(QObject *parent) const {
  return new QsciLexerMASM(parent);
}

QsciLexer *QScintillaLexerFactory::createNAsmLexer(QObject *parent) const {
  return new QsciLexerNASM(parent);
}

QsciLexer *QScintillaLexerFactory::createCMakeLexer(QObject *parent) const {
  return new QsciLexerCMake(parent);
}

QsciLexer *QScintillaLexerFactory::createDLexer(QObject *parent) const {
  return new QsciLexerD(parent);
}

QsciLexer *QScintillaLexerFactory::createPovLexer(QObject *parent) const {
  return new QsciLexerPOV(parent);
}

QsciLexer *QScintillaLexerFactory::createSpiceLexer(QObject *parent) const {
  return new QsciLexerSpice(parent);
}

QsciLexer *QScintillaLexerFactory::createLexerByType(const QString &lexerName,
                                                     QObject *parent) const {
  if (lexerName == "CPP")
    return createCppLexer(parent);
  if (lexerName == "Python")
    return createPythonLexer(parent);
  if (lexerName == "Java")
    return createJavaLexer(parent);
  if (lexerName == "TypeScript")
    return createTypeScriptLexer(parent);
  if (lexerName == "TSX")
    return createTSXLexer(parent);
  if (lexerName == "CSharp")
    return createCSharpLexer(parent);
  if (lexerName == "Dart")
    return createDartLexer(parent);
  if (lexerName == "Rust")
    return createRustLexer(parent);
  if (lexerName == "OCaml")
    return createOCamlLexer(parent);
  if (lexerName == "OCamlInterface")
    return createOCamlInterfaceLexer(parent);
  if (lexerName == "OCamlType")
    return createOCamlTypeLexer(parent);
  if (lexerName == "Haskell")
    return createHaskellLexer(parent);
  if (lexerName == "FSharp")
    return createFSharpLexer(parent);
  if (lexerName == "Scala")
    return createScalaLexer(parent);
  if (lexerName == "PowerShell")
    return createPowerShellLexer(parent);
  if (lexerName == "JavaScript")
    return createJavaScriptLexer(parent);
  if (lexerName == "HTML")
    return createHtmlLexer(parent);
  if (lexerName == "CSS")
    return createCssLexer(parent);
  if (lexerName == "XML")
    return createXmlLexer(parent);
  if (lexerName == "JSON")
    return createJsonLexer(parent);
  if (lexerName == "SQL")
    return createSqlLexer(parent);
  if (lexerName == "Bash")
    return createBashLexer(parent);

  if (lexerName == "Batch")
    return createBatchLexer(parent);
  if (lexerName == "Makefile")
    return createMakefileLexer(parent);
  if (lexerName == "Diff")
    return createDiffLexer(parent);
  if (lexerName == "Properties")
    return createPropertiesLexer(parent);
  if (lexerName == "YAML")
    return createYamlLexer(parent);
  if (lexerName == "Markdown")
    return createMarkdownLexer(parent);
  if (lexerName == "TeX")
    return createTexLexer(parent);
  if (lexerName == "PostScript")
    return createPostScriptLexer(parent);
  if (lexerName == "Matlab")
    return createMatlabLexer(parent);
  if (lexerName == "Octave")
    return createOctaveLexer(parent);
  if (lexerName == "Perl")
    return createPerlLexer(parent);
  if (lexerName == "Ruby")
    return createRubyLexer(parent);
  if (lexerName == "Go")
    return createGoLexer(parent);
  if (lexerName == "Zig")
    return createZigLexer(parent);
  if (lexerName == "Kotlin")
    return createKotlinLexer(parent);
  if (lexerName == "ObjectiveC")
    return createObjectiveCLexer(parent);
  if (lexerName == "Swift")
    return createSwiftLexer(parent);
  if (lexerName == "Fortran")
    return createFortranLexer(parent);
  if (lexerName == "Fortran77")
    return createFortran77Lexer(parent);
  if (lexerName == "IDL")
    return createIdlLexer(parent);
  if (lexerName == "VHDL")
    return createVhdlLexer(parent);
  if (lexerName == "Verilog")
    return createVerilogLexer(parent);
  if (lexerName == "TCL")
    return createTclLexer(parent);
  if (lexerName == "Lua")
    return createLuaLexer(parent);
  if (lexerName == "Pascal")
    return createPascalLexer(parent);
  if (lexerName == "Asm")
    return createAsmLexer(parent);
  if (lexerName == "MASM")
    return createMAsmLexer(parent);
  if (lexerName == "NASM")
    return createNAsmLexer(parent);
  if (lexerName == "CMake")
    return createCMakeLexer(parent);
  if (lexerName == "D")
    return createDLexer(parent);
  if (lexerName == "POV")
    return createPovLexer(parent);
  if (lexerName == "Spice")
    return createSpiceLexer(parent);

  qDebug() << "QScintillaLexerFactory: Unknown lexer name:" << lexerName;
  return nullptr;
}

void QScintillaLexerFactory::initializeExtensionMappings() {
  _extensionToLexer.clear();
  _filenameToLexer.clear();

  // C/C++ family
  _extensionToLexer["c"] = "CPP";
  _extensionToLexer["cpp"] = "CPP";
  _extensionToLexer["cxx"] = "CPP";
  _extensionToLexer["cc"] = "CPP";
  _extensionToLexer["c++"] = "CPP";
  _extensionToLexer["h"] = "CPP";
  _extensionToLexer["hpp"] = "CPP";
  _extensionToLexer["hxx"] = "CPP";
  _extensionToLexer["hh"] = "CPP";
  _extensionToLexer["h++"] = "CPP";

  // Python
  _extensionToLexer["py"] = "Python";
  _extensionToLexer["pyi"] = "Python";
  _extensionToLexer["pyw"] = "Python";
  _extensionToLexer["pyx"] = "Python";

  // Java and JVM-related
  _extensionToLexer["java"] = "Java";

  // JavaScript/TypeScript
  _extensionToLexer["js"] = "JavaScript";
  _extensionToLexer["jsx"] = "JavaScript";
  _extensionToLexer["mjs"] = "JavaScript";
  _extensionToLexer["ts"] = "TypeScript";
  _extensionToLexer["tsx"] = "TSX";

  // C# / Dart / Rust / OCaml / Haskell / PowerShell
  _extensionToLexer["cs"] = "CSharp";
  _extensionToLexer["csx"] = "CSharp";
  _extensionToLexer["dart"] = "Dart";
  _extensionToLexer["rs"] = "Rust";
  _extensionToLexer["ml"] = "OCaml";
  _extensionToLexer["mli"] = "OCamlInterface";
  _extensionToLexer["mlt"] = "OCamlType";
  _extensionToLexer["hs"] = "Haskell";
  _extensionToLexer["lhs"] = "Haskell";
  _extensionToLexer["fs"] = "FSharp";
  _extensionToLexer["fsi"] = "FSharp";
  _extensionToLexer["fsx"] = "FSharp";
  _extensionToLexer["fsscript"] = "FSharp";
  _extensionToLexer["scala"] = "Scala";
  _extensionToLexer["sc"] = "Scala";
  _extensionToLexer["ps1"] = "PowerShell";
  _extensionToLexer["psm1"] = "PowerShell";
  _extensionToLexer["psd1"] = "PowerShell";

  // Web Technologies
  _extensionToLexer["html"] = "HTML";
  _extensionToLexer["htm"] = "HTML";
  _extensionToLexer["xhtml"] = "HTML";
  _extensionToLexer["shtml"] = "HTML";
  _extensionToLexer["css"] = "CSS";
  _extensionToLexer["scss"] = "CSS";
  _extensionToLexer["sass"] = "CSS";
  _extensionToLexer["less"] = "CSS";

  // XML family
  _extensionToLexer["xml"] = "XML";
  _extensionToLexer["xsl"] = "XML";
  _extensionToLexer["xslt"] = "XML";
  _extensionToLexer["xsd"] = "XML";
  _extensionToLexer["svg"] = "XML";
  _extensionToLexer["xaml"] = "XML";

  // Data formats
  _extensionToLexer["json"] = "JSON";
  _extensionToLexer["jsonc"] = "JSON";
  _extensionToLexer["yaml"] = "YAML";
  _extensionToLexer["yml"] = "YAML";
  _extensionToLexer["properties"] = "Properties";
  _extensionToLexer["ini"] = "Properties";
  _extensionToLexer["cfg"] = "Properties";
  _extensionToLexer["conf"] = "Properties";

  // Database
  _extensionToLexer["sql"] = "SQL";
  _extensionToLexer["mysql"] = "SQL";
  _extensionToLexer["pgsql"] = "SQL";
  _extensionToLexer["plsql"] = "SQL";

  // Shell/Scripting
  _extensionToLexer["sh"] = "Bash";
  _extensionToLexer["bash"] = "Bash";
  _extensionToLexer["zsh"] = "Bash";
  _extensionToLexer["fish"] = "Bash";
  _extensionToLexer["bat"] = "Batch";
  _extensionToLexer["cmd"] = "Batch";

  // Build systems
  _extensionToLexer["makefile"] = "Makefile";
  _extensionToLexer["mak"] = "Makefile";
  _extensionToLexer["mk"] = "Makefile";
  _extensionToLexer["cmake"] = "CMake";
  _extensionToLexer["cmakelists"] = "CMake";

  // Documentation
  _extensionToLexer["md"] = "Markdown";
  _extensionToLexer["markdown"] = "Markdown";
  _extensionToLexer["mdown"] = "Markdown";
  _extensionToLexer["mkd"] = "Markdown";
  _extensionToLexer["tex"] = "TeX";
  _extensionToLexer["latex"] = "TeX";
  _extensionToLexer["sty"] = "TeX";
  _extensionToLexer["cls"] = "TeX";

  // Version control
  _extensionToLexer["diff"] = "Diff";
  _extensionToLexer["patch"] = "Diff";

  // PostScript
  _extensionToLexer["ps"] = "PostScript";
  _extensionToLexer["eps"] = "PostScript";

  // Scientific Computing
  _extensionToLexer["m"] = "Matlab";
  _extensionToLexer["matlab"] = "Matlab";
  _extensionToLexer["octave"] = "Octave";

  // Scripting Languages
  _extensionToLexer["pl"] = "Perl";
  _extensionToLexer["pm"] = "Perl";
  _extensionToLexer["perl"] = "Perl";
  _extensionToLexer["rb"] = "Ruby";
  _extensionToLexer["ruby"] = "Ruby";
  _extensionToLexer["rbw"] = "Ruby";
  _extensionToLexer["go"] = "Go";
  _extensionToLexer["zig"] = "Zig";
  _extensionToLexer["kt"] = "Kotlin";
  _extensionToLexer["kts"] = "Kotlin";
  _extensionToLexer["mm"] = "ObjectiveC";
  _extensionToLexer["swift"] = "Swift";
  _extensionToLexer["lua"] = "Lua";
  _extensionToLexer["tcl"] = "TCL";
  _extensionToLexer["tk"] = "TCL";

  // Systems Programming
  _extensionToLexer["f"] = "Fortran77";
  _extensionToLexer["for"] = "Fortran77";
  _extensionToLexer["f77"] = "Fortran77";
  _extensionToLexer["f90"] = "Fortran";
  _extensionToLexer["f95"] = "Fortran";
  _extensionToLexer["f03"] = "Fortran";
  _extensionToLexer["f08"] = "Fortran";
  _extensionToLexer["fortran"] = "Fortran";

  // Hardware Description
  _extensionToLexer["vhd"] = "VHDL";
  _extensionToLexer["vhdl"] = "VHDL";
  _extensionToLexer["v"] = "Verilog";
  _extensionToLexer["vh"] = "Verilog";
  _extensionToLexer["sv"] = "Verilog";

  // Data Analysis
  _extensionToLexer["idl"] = "IDL";
  _extensionToLexer["pro"] = "IDL";

  // Classical Languages
  _extensionToLexer["pas"] = "Pascal";
  _extensionToLexer["pascal"] = "Pascal";
  _extensionToLexer["pp"] = "Pascal";
  _extensionToLexer["ada"] = "Ada";
  _extensionToLexer["adb"] = "Ada";
  _extensionToLexer["ads"] = "Ada";

  // Functional Languages
  _extensionToLexer["lisp"] = "Lisp";
  _extensionToLexer["lsp"] = "Lisp";
  _extensionToLexer["cl"] = "Lisp";
  _extensionToLexer["scm"] = "Scheme";
  _extensionToLexer["scheme"] = "Scheme";
  _extensionToLexer["ss"] = "Scheme";

  // Assembly
  _extensionToLexer["asm"] = "Asm";
  _extensionToLexer["s"] = "Asm";
  _extensionToLexer["inc"] = "NASM";

  // Modern Languages
  _extensionToLexer["d"] = "D";
  _extensionToLexer["di"] = "D";

  // 3D Graphics
  _extensionToLexer["pov"] = "POV";
  _extensionToLexer["povray"] = "POV";
  _extensionToLexer["inc"] =
      "POV"; // Note: conflicts with assembly, POV takes precedence

  // Circuit Simulation
  _extensionToLexer["sp"] = "Spice";
  _extensionToLexer["spice"] = "Spice";
  _extensionToLexer["cir"] = "Spice";
  _extensionToLexer["net"] = "Spice";
}

void QScintillaLexerFactory::initializeFilenameMappings() {
  _filenameToLexer.clear();

  // Build/configuration files where the filename matters more than suffixes.
  _filenameToLexer["cmakelists.txt"] = "CMake";
  _filenameToLexer["makefile"] = "Makefile";
  _filenameToLexer["gnumakefile"] = "Makefile";
  _filenameToLexer["bsdmakefile"] = "Makefile";
  _filenameToLexer["dockerfile"] = "Bash";
  _filenameToLexer["containerfile"] = "Bash";
  _filenameToLexer["gitignore"] = "Properties";
  _filenameToLexer["gitattributes"] = "Properties";
  _filenameToLexer["gitmodules"] = "Properties";
  _filenameToLexer["editorconfig"] = "Properties";
  _filenameToLexer["readme"] = "Markdown";
  _filenameToLexer["readme.md"] = "Markdown";
  _filenameToLexer["changelog"] = "Markdown";
  _filenameToLexer["news"] = "Markdown";
  _filenameToLexer["license"] = "Markdown";
  _filenameToLexer["copying"] = "Markdown";
  _filenameToLexer["procfile"] = "Bash";
  _filenameToLexer["gemfile"] = "Ruby";
  _filenameToLexer["rakefile"] = "Ruby";
  _filenameToLexer["vagrantfile"] = "Ruby";
  _filenameToLexer["brewfile"] = "Ruby";
  _filenameToLexer["justfile"] = "Makefile";
  _filenameToLexer["jenkinsfile"] = "Custom";
  _filenameToLexer["meson.build"] = "Python";
  _filenameToLexer["meson_options.txt"] = "Python";
}

void QScintillaLexerFactory::initializeLexerDescriptions() {
  _lexerDescriptions.clear();

  _lexerDescriptions["CPP"] = "C/C++ Programming Language";
  _lexerDescriptions["Python"] = "Python Programming Language";
  _lexerDescriptions["Java"] = "Java Programming Language";
  _lexerDescriptions["TypeScript"] = "TypeScript Programming Language";
  _lexerDescriptions["TSX"] = "TypeScript JSX Programming Language";
  _lexerDescriptions["CSharp"] = "C# Programming Language";
  _lexerDescriptions["Dart"] = "Dart Programming Language";
  _lexerDescriptions["Rust"] = "Rust Programming Language";
  _lexerDescriptions["OCaml"] = "OCaml Programming Language";
  _lexerDescriptions["OCamlInterface"] = "OCaml Interface File";
  _lexerDescriptions["OCamlType"] = "OCaml Type File";
  _lexerDescriptions["Haskell"] = "Haskell Programming Language";
  _lexerDescriptions["FSharp"] = "F# Programming Language";
  _lexerDescriptions["Scala"] = "Scala Programming Language";
  _lexerDescriptions["PowerShell"] = "PowerShell Scripting Language";
  _lexerDescriptions["JavaScript"] = "JavaScript Programming Language";
  _lexerDescriptions["HTML"] = "HyperText Markup Language";
  _lexerDescriptions["CSS"] = "Cascading Style Sheets";
  _lexerDescriptions["XML"] = "Extensible Markup Language";
  _lexerDescriptions["JSON"] = "JavaScript Object Notation";
  _lexerDescriptions["SQL"] = "Structured Query Language";
  _lexerDescriptions["Bash"] = "Bash Shell Script";
  _lexerDescriptions["Batch"] = "Windows Batch Script";
  _lexerDescriptions["Makefile"] = "GNU Make Build Script";
  _lexerDescriptions["Diff"] = "Unified Diff Format";
  _lexerDescriptions["Properties"] = "Java Properties/INI Configuration";
  _lexerDescriptions["YAML"] = "YAML Ain't Markup Language";
  _lexerDescriptions["Markdown"] = "Markdown Documentation";
  _lexerDescriptions["TeX"] = "TeX/LaTeX Typesetting";
  _lexerDescriptions["PostScript"] = "PostScript Page Description";
  _lexerDescriptions["Matlab"] = "MATLAB Programming Language";
  _lexerDescriptions["Octave"] = "GNU Octave Programming Language";
  _lexerDescriptions["Perl"] = "Perl Programming Language";
  _lexerDescriptions["Ruby"] = "Ruby Programming Language";
  _lexerDescriptions["Go"] = "Go Programming Language";
  _lexerDescriptions["Zig"] = "Zig Programming Language";
  _lexerDescriptions["Kotlin"] = "Kotlin Programming Language";
  _lexerDescriptions["ObjectiveC"] = "Objective-C Programming Language";
  _lexerDescriptions["Swift"] = "Swift Programming Language";
  _lexerDescriptions["Fortran"] = "Fortran 90+ Programming Language";
  _lexerDescriptions["Fortran77"] = "Fortran 77 Programming Language";
  _lexerDescriptions["IDL"] = "Interactive Data Language";
  _lexerDescriptions["VHDL"] = "VHSIC Hardware Description Language";
  _lexerDescriptions["Verilog"] = "Verilog Hardware Description Language";
  _lexerDescriptions["TCL"] = "Tool Command Language";
  _lexerDescriptions["Lua"] = "Lua Scripting Language";
  _lexerDescriptions["Pascal"] = "Pascal Programming Language";
  _lexerDescriptions["Ada"] = "Ada Programming Language";
  _lexerDescriptions["Lisp"] = "Lisp Programming Language";
  _lexerDescriptions["Scheme"] = "Scheme Programming Language";
  _lexerDescriptions["Asm"] = "Assembly Language";
  _lexerDescriptions["MASM"] = "MASM Assembly Language";
  _lexerDescriptions["NASM"] = "NASM Assembly Language";
  _lexerDescriptions["CMake"] = "CMake Build System";
  _lexerDescriptions["D"] = "D Programming Language";
  _lexerDescriptions["POV"] = "POV-Ray Scene Description Language";
  _lexerDescriptions["Spice"] = "SPICE Circuit Simulation";
  _lexerDescriptions["Custom"] = "Custom User-Defined Language";
}
