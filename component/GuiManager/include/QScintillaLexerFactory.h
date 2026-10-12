#ifndef QSCINTILLALEXERFACTORY_H
#define QSCINTILLALEXERFACTORY_H

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

class QsciLexer;

namespace GuiManager {

/**
 * @brief The QScintillaLexerFactory class creates appropriate QScintilla lexers
 * based on file extensions and provides comprehensive language support.
 */
class QScintillaLexerFactory : public QObject {
  Q_OBJECT

public:
  explicit QScintillaLexerFactory(QObject *parent = nullptr);
  ~QScintillaLexerFactory();

  // Main factory methods
  QsciLexer *createLexerForFile(const QString &filePath,
                                QObject *parent = nullptr) const;
  QsciLexer *createLexerForExtension(const QString &extension,
                                     QObject *parent = nullptr) const;
  QsciLexer *createLexerByName(const QString &lexerName,
                               QObject *parent = nullptr) const;

  // Returns a lexer only for files the diff viewers can highlight with
  // tree-sitter, nullptr otherwise.
  static QsciLexer *createDiffViewerLexerForFile(const QString &filePath,
                                                 QObject *parent = nullptr);

  // Information methods
  QStringList supportedExtensions() const;
  QStringList supportedLexerNames() const;
  QString getLexerNameForExtension(const QString &extension) const;
  QString getDescriptionForExtension(const QString &extension) const;
  bool isExtensionSupported(const QString &extension) const;

  // Extension mapping management
  void addExtensionMapping(const QString &extension, const QString &lexerName);
  void removeExtensionMapping(const QString &extension);
  void setExtensionMapping(const QString &extension, const QString &lexerName);

  // Utility methods
  static QString extractExtension(const QString &filePath);
  static QString normalizeExtension(const QString &extension);
  static QString normalizeFileName(const QString &fileName);

public slots:
  void refreshMappings();

signals:
  void lexerCreated(QsciLexer *lexer, const QString &extension) const;
  void mappingChanged(const QString &extension, const QString &lexerName) const;

private:
  // Lexer creation methods
  QsciLexer *createCppLexer(QObject *parent) const;
  QsciLexer *createPythonLexer(QObject *parent) const;
  QsciLexer *createJavaLexer(QObject *parent) const;
  QsciLexer *createTypeScriptLexer(QObject *parent) const;
  QsciLexer *createTSXLexer(QObject *parent) const;
  QsciLexer *createCSharpLexer(QObject *parent) const;
  QsciLexer *createDartLexer(QObject *parent) const;
  QsciLexer *createRustLexer(QObject *parent) const;
  QsciLexer *createOCamlLexer(QObject *parent) const;
  QsciLexer *createOCamlInterfaceLexer(QObject *parent) const;
  QsciLexer *createOCamlTypeLexer(QObject *parent) const;
  QsciLexer *createHaskellLexer(QObject *parent) const;
  QsciLexer *createFSharpLexer(QObject *parent) const;
  QsciLexer *createScalaLexer(QObject *parent) const;
  QsciLexer *createPowerShellLexer(QObject *parent) const;
  QsciLexer *createJavaScriptLexer(QObject *parent) const;
  QsciLexer *createHtmlLexer(QObject *parent) const;
  QsciLexer *createCssLexer(QObject *parent) const;
  QsciLexer *createXmlLexer(QObject *parent) const;
  QsciLexer *createJsonLexer(QObject *parent) const;
  QsciLexer *createSqlLexer(QObject *parent) const;
  QsciLexer *createBashLexer(QObject *parent) const;
  QsciLexer *createBatchLexer(QObject *parent) const;
  QsciLexer *createMakefileLexer(QObject *parent) const;
  QsciLexer *createDiffLexer(QObject *parent) const;
  QsciLexer *createPropertiesLexer(QObject *parent) const;
  QsciLexer *createYamlLexer(QObject *parent) const;
  QsciLexer *createMarkdownLexer(QObject *parent) const;
  QsciLexer *createTexLexer(QObject *parent) const;
  QsciLexer *createPostScriptLexer(QObject *parent) const;
  QsciLexer *createMatlabLexer(QObject *parent) const;
  QsciLexer *createOctaveLexer(QObject *parent) const;
  QsciLexer *createPerlLexer(QObject *parent) const;
  QsciLexer *createRubyLexer(QObject *parent) const;
  QsciLexer *createGoLexer(QObject *parent) const;
  QsciLexer *createFortranLexer(QObject *parent) const;
  QsciLexer *createFortran77Lexer(QObject *parent) const;
  QsciLexer *createIdlLexer(QObject *parent) const;
  QsciLexer *createVhdlLexer(QObject *parent) const;
  QsciLexer *createVerilogLexer(QObject *parent) const;
  QsciLexer *createTclLexer(QObject *parent) const;
  QsciLexer *createLuaLexer(QObject *parent) const;
  QsciLexer *createPascalLexer(QObject *parent) const;
  QsciLexer *createMAsmLexer(QObject *parent) const;
  QsciLexer *createNAsmLexer(QObject *parent) const;
  QsciLexer *createCMakeLexer(QObject *parent) const;
  QsciLexer *createDLexer(QObject *parent) const;
  QsciLexer *createPovLexer(QObject *parent) const;
  QsciLexer *createSpiceLexer(QObject *parent) const;

  // Helper methods
  void initializeExtensionMappings();
  void initializeFilenameMappings();
  void initializeLexerDescriptions();
  QsciLexer *createLexerByType(const QString &lexerName, QObject *parent) const;

  // Member variables
  QHash<QString, QString> _extensionToLexer;  // extension -> lexer name
  QHash<QString, QString> _filenameToLexer;    // filename -> lexer name
  QHash<QString, QString> _lexerDescriptions;  // lexer name -> description

  // Lexer information structure
  struct LexerInfo {
    QString name;
    QString description;
    QStringList extensions;
    bool isAvailable;
  };

  QHash<QString, LexerInfo> _lexerInfo;
};

} // namespace GuiManager

#endif // QSCINTILLALEXERFACTORY_H
