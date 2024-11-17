SCINTILLA = $(top_srcdir)/scintilla

SUBDIRS = include

noinst_LTLIBRARIES = libscintilla.la

AM_CXXFLAGS = -DNDEBUG -DGTK -DSCI_LEXER -DNO_CXX11_REGEX

.PHONY: marshallers

marshallers: $(SCINTILLA)/gtk/scintilla-marshal.list
	glib-genmarshal --prefix scintilla_marshal $(SCINTILLA)/gtk/scintilla-marshal.list --header > gtk-scintilla-marshal.h
	glib-genmarshal --prefix scintilla_marshal $(SCINTILLA)/gtk/scintilla-marshal.list --body > gtk-scintilla-marshal.c

gtk-scintilla-marshal.c: marshallers
gtk-scintilla-marshal.h: marshallers

ScintillaGTKWithSymbols.cxx:
	patch -p0 $(SCINTILLA)/gtk/ScintillaGTK.cxx ScintillaGTKWithSymbols.cxx.patch -o ScintillaGTKWithSymbols.cxx

SRCS = \
	$(SCINTILLA)/gtk/Converter.h \
	$(SCINTILLA)/gtk/PlatGTK.cxx \
	$(SCINTILLA)/gtk/ScintillaGTK.h \
	$(SCINTILLA)/gtk/ScintillaGTKAccessible.cxx \
	$(SCINTILLA)/gtk/ScintillaGTKAccessible.h \
	$(SCINTILLA)/gtk/Wrappers.h \
	$(SCINTILLA)/src/AutoComplete.cxx \
	$(SCINTILLA)/src/AutoComplete.h \
	$(SCINTILLA)/src/CallTip.cxx \
	$(SCINTILLA)/src/CallTip.h \
	$(SCINTILLA)/src/CaseConvert.cxx \
	$(SCINTILLA)/src/CaseConvert.h \
	$(SCINTILLA)/src/CaseFolder.cxx \
	$(SCINTILLA)/src/CaseFolder.h \
	$(SCINTILLA)/src/CellBuffer.cxx \
	$(SCINTILLA)/src/CellBuffer.h \
	$(SCINTILLA)/src/ChangeHistory.cxx \
	$(SCINTILLA)/src/ChangeHistory.h \
	$(SCINTILLA)/src/CharClassify.cxx \
	$(SCINTILLA)/src/CharClassify.h \
	$(SCINTILLA)/src/CharacterCategoryMap.cxx \
	$(SCINTILLA)/src/CharacterCategoryMap.h \
	$(SCINTILLA)/src/CharacterType.cxx \
	$(SCINTILLA)/src/CharacterType.h \
	$(SCINTILLA)/src/ContractionState.cxx \
	$(SCINTILLA)/src/ContractionState.h \
	$(SCINTILLA)/src/DBCS.cxx \
	$(SCINTILLA)/src/DBCS.h \
	$(SCINTILLA)/src/Debugging.h \
	$(SCINTILLA)/src/Decoration.cxx \
	$(SCINTILLA)/src/Decoration.h \
	$(SCINTILLA)/src/Document.cxx \
	$(SCINTILLA)/src/Document.h \
	$(SCINTILLA)/src/EditModel.cxx \
	$(SCINTILLA)/src/EditModel.h \
	$(SCINTILLA)/src/EditView.cxx \
	$(SCINTILLA)/src/EditView.h \
	$(SCINTILLA)/src/Editor.cxx \
	$(SCINTILLA)/src/Editor.h \
	$(SCINTILLA)/src/ElapsedPeriod.h \
	$(SCINTILLA)/src/Geometry.cxx \
	$(SCINTILLA)/src/Geometry.h \
	$(SCINTILLA)/src/Indicator.cxx \
	$(SCINTILLA)/src/Indicator.h \
	$(SCINTILLA)/src/KeyMap.cxx \
	$(SCINTILLA)/src/KeyMap.h \
	$(SCINTILLA)/src/LineMarker.cxx \
	$(SCINTILLA)/src/LineMarker.h \
	$(SCINTILLA)/src/MarginView.cxx \
	$(SCINTILLA)/src/MarginView.h \
	$(SCINTILLA)/src/Partitioning.h \
	$(SCINTILLA)/src/PerLine.cxx \
	$(SCINTILLA)/src/PerLine.h \
	$(SCINTILLA)/src/Platform.h \
	$(SCINTILLA)/src/Position.h \
	$(SCINTILLA)/src/PositionCache.cxx \
	$(SCINTILLA)/src/PositionCache.h \
	$(SCINTILLA)/src/RESearch.cxx \
	$(SCINTILLA)/src/RESearch.h \
	$(SCINTILLA)/src/RunStyles.cxx \
	$(SCINTILLA)/src/RunStyles.h \
	$(SCINTILLA)/src/ScintillaBase.cxx \
	$(SCINTILLA)/src/ScintillaBase.h \
	$(SCINTILLA)/src/Selection.cxx \
	$(SCINTILLA)/src/Selection.h \
	$(SCINTILLA)/src/SparseVector.h \
	$(SCINTILLA)/src/SplitVector.h \
	$(SCINTILLA)/src/Style.cxx \
	$(SCINTILLA)/src/Style.h \
	$(SCINTILLA)/src/UndoHistory.cxx \
	$(SCINTILLA)/src/UndoHistory.h \
	$(SCINTILLA)/src/UniConversion.cxx \
	$(SCINTILLA)/src/UniConversion.h \
	$(SCINTILLA)/src/UniqueString.cxx \
	$(SCINTILLA)/src/UniqueString.h \
	$(SCINTILLA)/src/ViewStyle.cxx \
	$(SCINTILLA)/src/ViewStyle.h \
	$(SCINTILLA)/src/XPM.cxx \
	$(SCINTILLA)/src/XPM.h \
	ScintillaGTKWithSymbols.cxx \
	gtk-scintilla-marshal.c \
	gtk-scintilla-marshal.h

libscintilla_la_SOURCES = $(SRCS)

AM_CPPFLAGS = -I$(SCINTILLA)/include -I$(SCINTILLA)/gtk -I$(SCINTILLA)/src \
	@GTK_CFLAGS@ @LIBGEANY_CFLAGS@

EXTRA_DIST = $(SCINTILLA)/gtk/scintilla-marshal.list $(SCINTILLA)/License.txt $(SCINTILLA)/README $(SCINTILLA)/version.txt
