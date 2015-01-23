#ifndef REPLPAD_H
#define REPLPAD_H

//
// replpad.h
// This file is part of Ren Garden
// Copyright (C) 2015 MetÆducation
//
// Ren Garden is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Ren Garden is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Ren Garden.  If not, see <http://www.gnu.org/licenses/>.
//
// See http://ren-garden.metaeducation.com for more information on this project
//


#include <utility>
#include <vector>
#include "optional/optional.hpp"

#include <QTextEdit>
#include <QElapsedTimer>
#include <QWaitCondition>
#include <QMutex>

#include "fakestdio.h"


class ReplPad;



///
/// SYNTAX-AWARE INTERFACE
///

//
// The journey of 1,000 miles begins with a single step.  Rather than tackle
// syntax highlighting immediately, these syntax-aware hooks are called by
// the ReplPad.  Due to the fact that RenConsole has sensitivity to things
// like "which Tab are you on", it actually implements these hooks with
// multiple inheritance--but they're broken off into a separate interface
// in case there is more modularization needed at some point.
//

class IReplPadSyntaxer {
public:
    // rangeForWholeToken() means that if | represents your cursor and you had:
    //
    //     print {Hello| World}
    //
    // Then the offset would realize you wanted to select from opening curly
    // brace to the closing curly brace, vs merely selecting hello (as a
    // default text edit might.)

    virtual std::pair<int, int> rangeForWholeToken(
        QString buffer, int offset
    ) const = 0;

    // Beginnings of a basic auto completion (e.g. with tab) interface.  It
    // is similar to rangeForWholeToken, but assumes you are asking for a
    // completion of token where the cursor is sitting at the index position
    // (there may be text after the token, such as from a previous completion)

    virtual std::pair<QString, int> autoComplete(
        QString const & token, int index, bool backwards
    ) const = 0;

    virtual ~IReplPadSyntaxer () {}
};



///
/// HOOKS FOR CUSTOMIZING READ-EVAL-PRINT-LOOP "WORKPAD"
///

//
// Virtual interface class implementing the hooks offered by ReplPad (but
// keeping you from having to derive directly from ReplPad, and hence not
// needing to derive indirectly from QTextEdit.  Again, this is implemented
// by the RenConsole via multiple inheritance.
//

class IReplPadHooks {
public:
    virtual bool isReadyToModify(QKeyEvent * event) = 0;
    virtual QString getPromptString() = 0;
    virtual void evaluate(QString const & input, bool meta) = 0;
    virtual void escape() = 0;

    virtual ~IReplPadHooks () {}
};



///
/// READ-EVAL-PRINT-LOOP "WORKPAD"
///

//
// The REPL-Pad (Read-Eval-Print-Loop) is a kind of "command prompt workspace"
// used by Ren Garden.  It is currently built on top of QTextEdit, although
// the goal is to isolate the dependency on QTextEdit to just this unit, such
// that it could be switched out for other implementations (web views, etc).
// Additionally, the RenConsole has multiple ReplPad instances as tabs.
//

class ReplPad : public QTextEdit
{
    Q_OBJECT

private:
    IReplPadHooks & hooks;
    IReplPadSyntaxer & syntaxer;

public:
    ReplPad (
        IReplPadHooks & hooks,
        IReplPadSyntaxer & syntaxer,
        QWidget * parent = nullptr
    );

public:
    friend class FakeStdoutBuffer;
    FakeStdout fakeOut;

    friend class FakeStdinBuffer;
    FakeStdin fakeIn;

private:
    QMutex inputMutex;
    QWaitCondition inputAvailable;
    QByteArray input; // as utf-8

signals:
    void needGuiThreadTextAppend(QString text, bool centered);
    void needGuiThreadHtmlAppend(QString html, bool centered);
    void needGuiThreadImageAppend(QImage image, bool centered);
public:
    void appendImage(QImage const & image, bool centered = false);
    void appendText(QString const & text, bool centered = false);
    void appendHtml(QString const & html, bool centered = false);

private slots:
    void onRequestInput();


public:
    QTextCharFormat promptFormatNormal;
    QTextCharFormat promptFormatMeta;
    QTextCharFormat inputFormatNormal;
    QTextCharFormat inputFormatMeta;
    QTextCharFormat outputFormat;
    QTextCharFormat errorFormat;

    QTextBlockFormat leftFormat;
    QTextBlockFormat centeredFormat;

public:
    QFont defaultFont;

private:
    // QTextEdit has a ZoomIn and a ZoomOut that we can call, but it doesn't
    // have a way to query it.  So we keep track of the net number of zoom
    // in and zoom out calls that hvae been made, so we can save it in the
    // preferences file to restore for the next session
    int zoomDelta;
public:
    int getZoom();
    void setZoom(int delta);

protected:
    QMutex documentMutex;
signals:
    void salvageDocument();

    // Fade out signaling sent to main window for terminating GUI when you
    // hit or release the escape key.
signals:
    void fadeOutToQuit(bool escaping);

private:
    bool shouldFollow;
    bool isFormatPending;
    QTextCursor endCursor() const;
    QTextCharFormat pendingFormat;
public:
    void pushFormat(QTextCharFormat const & format);
protected slots:
    void followLatestOutput();
    void dontFollowLatestOutput();

protected:
    void clearCurrentInput();
    void containInputSelection();


protected:
    void keyPressEvent(QKeyEvent * event) override;
    void keyReleaseEvent(QKeyEvent * event) override;
    void mousePressEvent(QMouseEvent * event) override;
    void mouseDoubleClickEvent(QMouseEvent * event) override;

    // Trap cut, paste, del so we can limit the selection to the edit buffer
public slots:
    void cutSafely();
    void pasteSafely();

signals:
    void reportStatus(QString const & str);

private:
    void rewritePrompt();

private:
    // "Escape" in this sense refers to "escaping the current mode".  The
    // way it is achieved by default is by pressing Qt::Key_Escape twice
    // within the double click period, but it is more a "conceptual escape"
    // that the virtual method is to implement.

    QElapsedTimer escapeTimer;

private:
    bool hasUndo;
    bool hasRedo;
    // if using up/down navigation, which index you're on ATM
    std::experimental::optional<size_t> historyIndex;
    class HistoryEntry {
    public:
        int promptPos;
        int inputPos;
        bool multiline;
        bool meta;
        std::experimental::optional<int> position; // offset from inputPos
        std::experimental::optional<int> anchor; // offset from inputPos
        std::experimental::optional<int> endPos;
    public:
        HistoryEntry (int promptPos) :
            promptPos (promptPos),
            inputPos (promptPos), // temporary; quickly overwritten
            multiline (false),
            meta (false),
            position (),
            anchor (),
            endPos ()
        {
        }
    public:
        QString getInput(ReplPad & pad) const;
    };
    std::vector<HistoryEntry> history;

public:
    void appendNewPrompt();

    void setBuffer(QString const & text, int position, int anchor);
};

#endif
