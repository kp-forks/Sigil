/************************************************************************
**
**  Copyright (C) 2015-2025 Kevin B. Hendricks, Stratford, ON
**  Copyright (C) 2012      John Schember <john@nachtimwald.com>
**  Copyright (C) 2012      Dave Heiland
**
**  This file is part of Sigil.
**
**  Sigil is free software: you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation, either version 3 of the License, or
**  (at your option) any later version.
**
**  Sigil is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with Sigil.  If not, see <http://www.gnu.org/licenses/>.
**
*************************************************************************/

#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtWidgets/QFileDialog>
#include <QtGui/QFont>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>

#include "sigil_exception.h"
#include "BookManipulation/FolderKeeper.h"
#include "Dialogs/ReportsWidgets/HTMLFilesWidget.h"
#include "Misc/HTMLSpellCheck.h"
#include "Misc/NumericItem.h"
#include "Misc/SettingsStore.h"
#include "Misc/Utility.h"
#include "ResourceObjects/HTMLResource.h"
#include "ResourceObjects/Resource.h"

static const QString SETTINGS_GROUP = "reports";
static const QString DEFAULT_REPORT_FILE = "HTMLFilesReport.csv";


HTMLFilesWidget::HTMLFilesWidget()
    :
    m_ItemModel(new QStandardItemModel),
    m_ContextMenu(new QMenu(this)),
    m_LastDirSaved(QString()),
    m_LastFileSaved(QString())
{
    ui.setupUi(this);
    ui.fileTree->setContextMenuPolicy(Qt::CustomContextMenu);
    CreateContextMenuActions();
    connectSignalsSlots();
}

HTMLFilesWidget::~HTMLFilesWidget()
{
    delete m_ItemModel;
}

void HTMLFilesWidget::CreateReport(QSharedPointer<Book> book)
{
    m_Book = book;
    SetupTable();
}

void HTMLFilesWidget::SetupTable(int sort_column, Qt::SortOrder sort_order)
{
    // Need to rebuild m_HTMLResources since deletes can happen behind the scenes
    m_HTMLResources = m_Book->GetFolderKeeper()->GetResourceTypeList<HTMLResource>(false);
    m_ItemModel->clear();
    QStringList header;
    header.append(tr("Name"));
    header.append(tr("File Size (KB)"));
    header.append(tr("All Words"));
    header.append(tr("Misspelled Words"));
    header.append(tr("Images"));
    header.append(tr("Video"));
    header.append(tr("Audio"));
    header.append(tr("Stylesheets"));
    header.append(tr("Well Formed"));
    m_ItemModel->setHorizontalHeaderLabels(header);
    ui.fileTree->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui.fileTree->setModel(m_ItemModel);
    ui.fileTree->header()->setSortIndicatorShown(true);
    bool wellformed = true;
    double total_size = 0;
    int total_all_words = 0;
    int total_misspelled_words = 0;
    int total_images = 0;
    int total_video = 0;
    int total_audio = 0;
    int total_stylesheets = 0;
    int total_wellformed = 0;
    QHash<QString, QStringList> stylesheet_names_hash = m_Book->GetStylesheetsInHTMLFiles();
    QHash<QString, QStringList> image_names_hash = m_Book->GetImagesInHTMLFiles();
    QHash<QString, QStringList> video_names_hash = m_Book->GetVideoInHTMLFiles();
    QHash<QString, QStringList> audio_names_hash = m_Book->GetAudioInHTMLFiles();
    QHash<QString, std::pair<int, int> > word_count_hash = m_Book->GetSpellWordCountsInHTMLFiles();
    foreach(HTMLResource *html_resource, m_HTMLResources) {
        QString filepath = html_resource->GetRelativePath();
        QString path = html_resource->GetFullPath();
        QString filename = html_resource->ShortPathName();
        QList<QStandardItem *> rowItems;
        // Filename
        QStandardItem *name_item = new QStandardItem();
        name_item->setText(filename);
        name_item->setToolTip(filepath);
        name_item->setData(filepath);
        rowItems << name_item;
        // File Size
        double ffsize = QFile(path).size() / 1024.0;
        total_size += ffsize;
        QString fsize = QLocale().toString(ffsize, 'f', 2);
        NumericItem *size_item = new NumericItem();
        size_item->setText(fsize);
        size_item->setTextAlignment(Qt::AlignRight);
        rowItems << size_item;
        // All words
        std::pair<int, int> counts = word_count_hash[filepath];
        total_all_words += counts.first;
        NumericItem *words_item = new NumericItem();
        words_item->setText(QString("%L1").arg(counts.first));
        words_item->setTextAlignment(Qt::AlignRight);
        rowItems << words_item;
        // Misspelled words
        total_misspelled_words += counts.second;
        NumericItem *misspelled_item = new NumericItem();
        misspelled_item->setText(QString("%L1").arg(counts.second));
        misspelled_item->setTextAlignment(Qt::AlignRight);
        rowItems << misspelled_item;
        // Images
        NumericItem *image_item = new NumericItem();
        QStringList image_names = image_names_hash[filepath];
        total_images += image_names.count();
        image_item->setText(QString("%L1").arg(image_names.count()));
        image_item->setTextAlignment(Qt::AlignRight);
        if (!image_names.isEmpty()) {
            image_item->setToolTip(image_names.join("\n"));
        }
        rowItems << image_item;
        // Video
        NumericItem *video_item = new NumericItem();
        QStringList video_names = video_names_hash[filepath];
        total_video += video_names.count();
        video_item->setText(QString("%L1").arg(video_names.count()));
        video_item->setTextAlignment(Qt::AlignRight);
        if (!video_names.isEmpty()) {
            video_item->setToolTip(video_names.join("\n"));
        }
        rowItems << video_item;
        // Audio
        NumericItem *audio_item = new NumericItem();
        QStringList audio_names = audio_names_hash[filepath];
        total_audio += audio_names.count();
        audio_item->setText(QString("%L1").arg(audio_names.count()));
        audio_item->setTextAlignment(Qt::AlignRight);
        if (!audio_names.isEmpty()) {
            audio_item->setToolTip(audio_names.join("\n"));
        }
        rowItems << audio_item;
        // Linked Stylesheets
        NumericItem *stylesheet_item = new NumericItem();
        QStringList stylesheet_names = stylesheet_names_hash[filepath];
        total_stylesheets += stylesheet_names.count();
        stylesheet_item->setText(QString("%L1").arg(stylesheet_names.count()));
        stylesheet_item->setTextAlignment(Qt::AlignRight);
        if (!stylesheet_names.isEmpty()) {
            stylesheet_item->setToolTip(stylesheet_names.join("\n"));
        }
        rowItems << stylesheet_item;
        // Well formed
        QStandardItem *wellformed_item = new QStandardItem();
        wellformed = html_resource->FileIsWellFormed();
        if (wellformed) {
            total_wellformed++;
        }
        wellformed_item->setText(wellformed ? tr("Yes") : tr("No"));
        rowItems << wellformed_item;

        // Add item to table
        m_ItemModel->appendRow(rowItems);
        for (int i = 0; i < rowItems.count(); i++) {
            rowItems[i]->setEditable(false);
        }
    }
    // Sort before adding the totals row
    // Since sortIndicator calls this routine, must disconnect/reconnect while resorting
    disconnect(ui.fileTree->header(), SIGNAL(sortIndicatorChanged(int, Qt::SortOrder)), this, SLOT(Sort(int, Qt::SortOrder)));
    ui.fileTree->header()->setSortIndicator(sort_column, sort_order);
    connect(ui.fileTree->header(), SIGNAL(sortIndicatorChanged(int, Qt::SortOrder)), this, SLOT(Sort(int, Qt::SortOrder)));
    // Totals
    NumericItem *nitem;
    QList<QStandardItem *> rowItems;
    // Files
    nitem = new NumericItem();
    nitem->setText(QString(tr("%n file(s)", "", m_HTMLResources.count())));
    rowItems << nitem;
    // File size
    nitem = new NumericItem();
    nitem->setText(QLocale().toString(total_size, 'f', 2));
    nitem->setTextAlignment(Qt::AlignRight);
    rowItems << nitem;
    // All Words
    nitem = new NumericItem();
    nitem->setText(QString("%L1").arg(total_all_words));
    nitem->setTextAlignment(Qt::AlignRight);
    rowItems << nitem;
    // Misspelled Words
    nitem = new NumericItem();
    nitem->setText(QString("%L1").arg(total_misspelled_words));
    nitem->setTextAlignment(Qt::AlignRight);
    rowItems << nitem;
    // Images
    nitem = new NumericItem();
    nitem->setText(QString("%L1").arg(total_images));
    nitem->setTextAlignment(Qt::AlignRight);
    rowItems << nitem;
    // Video
    nitem = new NumericItem();
    nitem->setText(QString("%L1").arg(total_video));
    nitem->setTextAlignment(Qt::AlignRight);
    rowItems << nitem;
    // Audio
    nitem = new NumericItem();
    nitem->setText(QString("%L1").arg(total_audio));
    nitem->setTextAlignment(Qt::AlignRight);
    rowItems << nitem;
    // Stylesheets
    nitem = new NumericItem();
    nitem->setText(QString("%L1").arg(total_stylesheets));
    nitem->setTextAlignment(Qt::AlignRight);
    rowItems << nitem;
    // Well formed
    nitem = new NumericItem();
    nitem->setText(QString("%L1").arg(total_wellformed));
    rowItems << nitem;

    QFont font;
    font.setWeight(QFont::Bold);
    for (int i = 0; i < rowItems.count(); i++) {
        rowItems[i]->setEditable(false);
        rowItems[i]->setFont(font);
    }

    m_ItemModel->appendRow(rowItems);

    for (int i = 0; i < ui.fileTree->header()->count(); i++) {
        ui.fileTree->resizeColumnToContents(i);
    }
}


void HTMLFilesWidget::FilterEditTextChangedSlot(const QString &text)
{
    const QString lowercaseText = text.toLower();
    QStandardItem *root_item = m_ItemModel->invisibleRootItem();
    QModelIndex parent_index;
    // Hide rows that don't contain the filter text
    int first_visible_row = -1;

    for (int row = 0; row < root_item->rowCount(); row++) {
        if (text.isEmpty() || root_item->child(row, 0)->text().toLower().contains(lowercaseText)) {
            ui.fileTree->setRowHidden(row, parent_index, false);

            if (first_visible_row == -1) {
                first_visible_row = row;
            }
        } else {
            ui.fileTree->setRowHidden(row, parent_index, true);
        }
    }

    if (!text.isEmpty() && first_visible_row != -1) {
        // Select the first non-hidden row
        ui.fileTree->setCurrentIndex(root_item->child(first_visible_row, 0)->index());
    } else {
        // Clear current and selection, which clears preview image
        ui.fileTree->setCurrentIndex(QModelIndex());
    }
}

void HTMLFilesWidget::Sort(int logicalindex, Qt::SortOrder order)
{
    SetupTable(logicalindex, order);
}

void HTMLFilesWidget::DoubleClick()
{
    QModelIndex index = ui.fileTree->selectionModel()->selectedRows(0).first();

    if (index.row() != m_ItemModel->rowCount() - 1) {
        QString filepath = m_ItemModel->itemFromIndex(index)->data().toString();
        emit OpenFileRequest(filepath, 1, -1);
    }
}

void HTMLFilesWidget::Save()
{
    QStringList report_info;
    QStringList heading_row;

    // Get headings
    for (int col = 0; col < ui.fileTree->header()->count(); col++) {
        QStandardItem *item = m_ItemModel->horizontalHeaderItem(col);
        QString text = "";
        if (item) {
            text = item->text();
        }
        heading_row << text;
    }
    report_info << Utility::createCSVLine(heading_row);

    // Get data from table
    for (int row = 0; row < m_ItemModel->rowCount(); row++) {
        QStringList data_row;
        for (int col = 0; col < ui.fileTree->header()->count(); col++) {
            QStandardItem *item = m_ItemModel->item(row, col);
            QString text = "";
            if (item) {
                text = item->text();
            }
            data_row << text;
        }
        report_info << Utility::createCSVLine(data_row);
    }

    QString data = report_info.join('\n') + '\n';
    // Save the file
    ReadSettings();
    QString filter_string = "*.csv;;*.txt;;*.*";
    QString default_filter = "";
    QString save_path = m_LastDirSaved + "/" + m_LastFileSaved;
    QFileDialog::Options options = Utility::DlgOptions();

    QString destination = QFileDialog::getSaveFileName(this,
                                                       tr("Save Report As Comma Separated File"),
                                                       save_path,
                                                       filter_string,
                                                       &default_filter,
                                                       options);

    if (destination.isEmpty()) {
        return;
    }

    try {
        Utility::WriteUnicodeTextFile(data, destination);
    } catch (CannotOpenFile&) {
        Utility::warning(this, tr("Sigil"), tr("Cannot save report file."));
    }

    m_LastDirSaved = QFileInfo(destination).absolutePath();
    m_LastFileSaved = QFileInfo(destination).fileName();
    WriteSettings();
}

void HTMLFilesWidget::Delete()
{
    QStringList files_to_delete;

    if (ui.fileTree->selectionModel()->hasSelection()) {
        foreach(QModelIndex index, ui.fileTree->selectionModel()->selectedRows(0)) {
            QString bookpath = m_ItemModel->itemFromIndex(index)->data().toString();
            files_to_delete.append(bookpath);
        }
    }

    emit DeleteFilesRequest(files_to_delete);
    SetupTable();
}

void HTMLFilesWidget::CreateContextMenuActions()
{
    m_Delete    = new QAction(tr("Delete From Book") + "...",     this);
    m_Delete->setShortcut(QKeySequence::Delete);
    // Has to be added to the dialog itself for the keyboard shortcut to work.
    addAction(m_Delete);
}

void HTMLFilesWidget::OpenContextMenu(const QPoint &point)
{
    SetupContextMenu(point);
    m_ContextMenu->exec(ui.fileTree->viewport()->mapToGlobal(point));
    if (!m_ContextMenu.isNull()) {
        m_ContextMenu->clear();
    }
}

void HTMLFilesWidget::SetupContextMenu(const QPoint &point)
{
    m_ContextMenu->addAction(m_Delete);
    // We do not enable the delete option if no rows selected or the totals row is selected.
    m_Delete->setEnabled(ui.fileTree->selectionModel()->selectedRows().count() > 0);
    int last_row = ui.fileTree->model()->rowCount() - 1;

    if (ui.fileTree->selectionModel()->isRowSelected(last_row, QModelIndex())) {
        m_Delete->setEnabled(false);
    }
}

void HTMLFilesWidget::ReadSettings()
{
    SettingsStore settings;
    settings.beginGroup(SETTINGS_GROUP);
    // Last file open
    m_LastDirSaved = settings.value("last_dir_saved").toString();
    m_LastFileSaved = settings.value("last_file_saved_html_files").toString();

    if (m_LastFileSaved.isEmpty()) {
        m_LastFileSaved = DEFAULT_REPORT_FILE;
    }

    settings.endGroup();
}

void HTMLFilesWidget::WriteSettings()
{
    SettingsStore settings;
    settings.beginGroup(SETTINGS_GROUP);
    // Last file open
    settings.setValue("last_dir_saved", m_LastDirSaved);
    settings.setValue("last_file_saved_html_files", m_LastFileSaved);
    settings.endGroup();
}


void HTMLFilesWidget::connectSignalsSlots()
{
    connect(ui.leFilter,  SIGNAL(textChanged(QString)),
            this,         SLOT(FilterEditTextChangedSlot(QString)));
    connect(ui.fileTree, SIGNAL(doubleClicked(const QModelIndex &)),
            this,         SLOT(DoubleClick()));
    connect(ui.fileTree->header(), SIGNAL(sortIndicatorChanged(int, Qt::SortOrder)), this, SLOT(Sort(int, Qt::SortOrder)));
    connect(ui.fileTree,  SIGNAL(customContextMenuRequested(const QPoint &)),
            this,         SLOT(OpenContextMenu(const QPoint &)));
    connect(m_Delete,     SIGNAL(triggered()), this, SLOT(Delete()));
    connect(ui.buttonBox->button(QDialogButtonBox::Close), SIGNAL(clicked()), this, SIGNAL(CloseDialog()));
    connect(ui.buttonBox->button(QDialogButtonBox::Save), SIGNAL(clicked()), this, SLOT(Save()));
}

