#pragma once

namespace StylesheetManager {

enum class StylesheetKey {
  Unknown = 0,
  SourcererLight,
  SourcererDark,
  SourcererPurple,
  SourcererBlue,
  SourcererMonokai,
  SourcererGruvbox,
  SourcererSolarized,
  SourcererDracula,
  SourcererAyu,

  last,
  Default = SourcererDark
};

}
