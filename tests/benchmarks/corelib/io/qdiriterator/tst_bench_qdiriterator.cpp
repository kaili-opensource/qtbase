// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only
#include <QDebug>
#include <QDirIterator>
#include <QDirListing>
#include <QString>
#include <qplatformdefs.h>

#ifdef Q_OS_WIN
#   include <qt_windows.h>
#else
#   include <sys/stat.h>
#   include <sys/types.h>
#   include <dirent.h>
#   include <errno.h>
#   include <string.h>
#endif

#include <qtest.h>

#include "qfilesystemiterator.h"

#if QT_CONFIG(cxx17_filesystem)
#include <filesystem>
#endif

using namespace Qt::StringLiterals;

constexpr bool forceStat = false;

class tst_QDirIterator : public QObject
{
    Q_OBJECT

    void data();

    const QDir::Filters dirFilters =
            // QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot
            QDir::AllEntries | QDir::Hidden
            //QDir::Files | QDir::NoDotAndDotDot,
            // QDir::Files,
            ;

private slots:
    void posix();
    void posix_data() { data(); }
    void diriterator();
    void diriterator_data() { data(); }
    void dirlisting();
    void dirlisting_data() { data(); }
    void fsiterator();
    void fsiterator_data() { data(); }
    void stdRecursiveDirectoryIterator();
    void stdRecursiveDirectoryIterator_data() { data(); }
};

void tst_QDirIterator::data()
{
    const char hereRelative[] = "tests/benchmarks/corelib/io/qdiriterator";
    QByteArray dir(QT_TESTCASE_SOURCEDIR);
    // qDebug("Source dir: %s", dir.constData());
    dir.chop(sizeof(hereRelative)); // Counts the '\0', making up for the omitted leading '/'
    // qDebug("Root dir: %s", dir.constData());

    QTest::addColumn<QByteArray>("dirpath");
    const QByteArray ba = dir + "/src/corelib";

    if (!QFileInfo(QString::fromLocal8Bit(ba)).isDir())
        QSKIP("Missing Qt directory");

    QTest::newRow("corelib") << ba;
    QTest::newRow("corelib/io") << (ba + "/io");
}

#ifdef Q_OS_WIN
static int posix_helper(const wchar_t *dirpath, size_t length)
{
    int count = 0;
    HANDLE hSearch;
    WIN32_FIND_DATA fd;

    wchar_t appendedPath[MAX_PATH];
    Q_ASSERT(MAX_PATH > length + 3);
    wcsncpy(appendedPath, dirpath, length);
    wcscpy(appendedPath + length, L"\\*");
    hSearch = FindFirstFile(appendedPath, &fd);

    if (hSearch == INVALID_HANDLE_VALUE) {
        qWarning("FindFirstFile failed");
        return count;
    }

    do {
        if (!(fd.cFileName[0] == L'.' && fd.cFileName[1] == 0) &&
            !(fd.cFileName[0] == L'.' && fd.cFileName[1] == L'.' && fd.cFileName[2] == 0))
        {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                // newLength will "point" to where we put a * earlier, so we overwrite that.
                size_t newLength = length + 1; // "+ 1" for directory separator
                Q_ASSERT(newLength + wcslen(fd.cFileName) + 1 < MAX_PATH); // "+ 1" for null-terminator
                wcscpy(appendedPath + newLength, fd.cFileName);
                newLength += wcslen(fd.cFileName);
                count += posix_helper(appendedPath, newLength);
            }
            else {
                ++count;
            }
        }
    } while (FindNextFile(hSearch, &fd));
    FindClose(hSearch);

    return count;
}

#else

static int posix_helper(const char *dirpath)
{
    //qDebug() << "DIR" << dirpath;
    DIR *dir = ::opendir(dirpath);
    if (!dir)
        return 0;

    dirent *entry = 0;

    int count = 0;
    while ((entry = ::readdir(dir))) {
        if (qstrcmp(entry->d_name, ".") == 0)
            continue;
        if (qstrcmp(entry->d_name, "..") == 0)
            continue;
        ++count;
        QByteArray ba = dirpath;
        ba += '/';
        ba += entry->d_name;
        bool isDir = false;
#if defined(_DIRENT_HAVE_D_TYPE) || defined(Q_OS_BSD4)
        isDir = entry->d_type == DT_DIR;
        if (forceStat) {
            QT_STATBUF st;
            QT_LSTAT(ba.constData(), &st);
        }
#else // d_type not available >>> must stat() to see if it's a dir
        QT_STATBUF st;
        QT_LSTAT(ba.constData(), &st);
        isDir = S_ISDIR(st.st_mode);
#endif

        if (isDir)
            count += posix_helper(ba.constData());
    }

    ::closedir(dir);
    return count;
}
#endif


void tst_QDirIterator::posix()
{
    QFETCH(QByteArray, dirpath);

    int count = 0;
    QString path(dirpath);
    QBENCHMARK {
#ifdef Q_OS_WIN
        wchar_t wPath[MAX_PATH];
        const int end = path.toWCharArray(wPath);
        count = posix_helper(wPath, end);
#else
        count = posix_helper(dirpath.constData());
#endif
    }
    qDebug() << count;
}

void tst_QDirIterator::diriterator()
{
    QFETCH(QByteArray, dirpath);

    int count = 0;

    QBENCHMARK {
        int c = 0;

        QDirIterator dir(dirpath, dirFilters, QDirIterator::Subdirectories);

        while (dir.hasNext()) {
            const auto fi = dir.nextFileInfo();
            if (forceStat)
                fi.size();
            //printf("%s\n", qPrintable(dir.fileName()));
            0 && printf("%d %s\n",
                fi.isDir(),
                //qPrintable(fi.absoluteFilePath()),
                //qPrintable(dir.path()),
                qPrintable(fi.filePath()));
            ++c;
        }
        count = c;
    }
    qDebug() << count;
}

void tst_QDirIterator::dirlisting()
{
    QFETCH(QByteArray, dirpath);

    using F = QDirListing::IteratorFlag;

    int count = 0;

    QBENCHMARK {
        int c = 0;

        QDirListing dir(dirpath, F::Recursive | F::IncludeHidden);

        for (const auto &dirEntry : dir) {
            const auto path = dirEntry.filePath();
            if (forceStat)
                dirEntry.size();
            ++c;
        }
        count = c;
    }
    qDebug() << count;
}

void tst_QDirIterator::fsiterator()
{
    QFETCH(QByteArray, dirpath);

    int count = 0;
    int dump = 0;

    QBENCHMARK {
        int c = 0;

        dump && printf("\n\n\n\n");
        QDirIteratorTest::QFileSystemIterator dir(
            dirpath, dirFilters, QDirIteratorTest::QFileSystemIterator::Subdirectories);

        for (; !dir.atEnd(); dir.next()) {
            if (forceStat)
                dir.fileInfo().size();
            dump && printf("%d %s\n",
                dir.fileInfo().isDir(),
                //qPrintable(dir.fileInfo().absoluteFilePath()),
                //qPrintable(dir.path()),
                qPrintable(dir.filePath())
            );
            ++c;
        }
        count = c;
    }
    qDebug() << count;
}

void tst_QDirIterator::stdRecursiveDirectoryIterator()
{
#if QT_CONFIG(cxx17_filesystem)
    QFETCH(QByteArray, dirpath);

    int count = 0;

    namespace fs = std::filesystem;
    std::error_code ec;

    // Note that fs::recursive_directory_iterator may be calling stat() internally,
    // that depends on the implementation. So the benchmark times might "seem" higher
    // than the other methods in this source file.
    QBENCHMARK {
        int c = 0;
        for (const auto &dirEntry : fs::recursive_directory_iterator(dirpath.data(), ec)) {
            if (dirEntry.is_directory())
                continue;
            c++;
        }
        count = c;
    }
    qDebug() << count;
#else
    QSKIP("Not supported.");
#endif
}

QTEST_MAIN(tst_QDirIterator)

#include "tst_bench_qdiriterator.moc"
