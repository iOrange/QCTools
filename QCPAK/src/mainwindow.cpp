#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <Windows.h>

#include "aboutdlg.h"

#include <filesystem>
namespace fs = std::experimental::filesystem::v1;

#include <QFileDialog>
#include <QFile>
#include <QMessageBox>
#include <QProgressDialog>

Q_DECLARE_METATYPE(Directory*);

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_PAK(nullptr)
{
    ui->setupUi(this);

    QStringList labels;
    labels << "File name" << "File type" << "Compressed" << "Uncompressed" << "Offset";
    ui->filesList->setHeaderLabels(labels);
    ui->filesList->header()->resizeSection(0, 300);

    ui->treeWidget->setHeaderLabels(QStringList("PAK hierarchy"));
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::BuildDirectories() {
    if (m_PAK) {
        for (auto& d : m_Directories) {
            delete d;
        }
        m_Directories.clear();

        m_RootDir = new Directory();
        m_RootDir->name = m_PAK->GetPAKName();
        m_RootDir->fullPath = m_RootDir->name;
        m_Directories.push_back(m_RootDir);

        const qc::u64 numFiles = m_PAK->GetNumFiles();
        for (qc::u64 i = 0; i < numFiles; ++i) {
            Directory* dir = m_RootDir;

            const qc::String& name = m_PAK->GetFileName(i);
            const qc::String::size_type sepPos = name.find_last_of(Directory::DELIMITER);
            if (sepPos != qc::String::npos) {
                qc::String dirPath = name.substr(0, sepPos);
                dir = this->GetOrCreateDirectory(dirPath);
            }

            dir->files.push_back(i);
        }
    }
}

void MainWindow::AddDirRecursively(Directory* dir, QTreeWidgetItem* parent) {
    QTreeWidgetItem* item = nullptr;

    if (!parent) {
        item = new QTreeWidgetItem(QStringList(tr(m_PAK->GetPAKName().c_str())));
        ui->treeWidget->addTopLevelItem(item);
    } else {
        item = new QTreeWidgetItem(QStringList(dir->name.c_str()));
    }
    item->setIcon(0, QIcon(":/icons/folder_closed.png"));

    for (auto d : dir->directories) {
        this->AddDirRecursively(d, item);
    }

    item->setData(0, Qt::UserRole, QVariant::fromValue<Directory*>(dir));

    if (parent) {
        parent->addChild(item);
    }
}

void MainWindow::UpdateDirectoriesTree() {
    ui->treeWidget->clear();
    this->AddDirRecursively(m_RootDir, nullptr);
}

void MainWindow::UpdateFilesTable(Directory* dir) {
    ui->filesList->clear();

    for (const qc::u64 i : dir->files) {
        qc::String fullName = m_PAK->GetFileName(i);
        auto pos = fullName.find_last_of('\\');

        QString name = QString::fromStdString(fullName.substr(pos + 1));
        QString type = "***";
        QString comp = QString::number(m_PAK->GetFileCompressedSize(i));
        QString uncomp = QString::number(m_PAK->GetFileUncompressedSize(i));
        QString offset = QString::number(m_PAK->GetFileOffset(i));


        QTreeWidgetItem* item = new QTreeWidgetItem();
        item->setText(0, name);
        item->setText(1, type);
        item->setText(2, comp);
        item->setText(3, uncomp);
        item->setText(4, offset);
        item->setData(0, Qt::UserRole, i);

        ui->filesList->addTopLevelItem(item);
    }
}

Directory* MainWindow::GetOrCreateDirectory(const qc::String& fullPath) {
    Directory* result = nullptr;

    result = this->FindDirectory(fullPath);

    if (!result) {
        qc::String copyFull = fullPath + Directory::DELIMITER;
        qc::String::size_type sepPos = fullPath.find_first_of(Directory::DELIMITER);
        Directory* lastDir = m_RootDir;
        qc::String curParent;

        while (qc::String::npos != sepPos) {
            qc::String dir = copyFull.substr(0, sepPos);
            copyFull = copyFull.substr(sepPos + 1);

            sepPos = copyFull.find_first_of(Directory::DELIMITER);

            Directory* parent = this->FindDirectory(curParent);
            if (!parent) {
                parent = new Directory();
                parent->name = curParent;
                parent->fullPath = curParent;

                m_RootDir->directories.push_back(parent);
                m_Directories.push_back(parent);
            }
            qc::String newPath = (!curParent.empty()) ? (curParent + Directory::DELIMITER + dir) : dir;
            Directory* newDir = this->FindDirectory(newPath);
            if (!newDir) {
                newDir = new Directory();
                newDir->name = dir;
                newDir->fullPath = newPath;

                parent->directories.push_back(newDir);
                m_Directories.push_back(newDir);
            }
            lastDir = newDir;
            curParent = newPath;
        }

        result = lastDir;
    }

    return result;
}

Directory* MainWindow::FindDirectory(const qc::String& fullPath) {
    Directory* result = nullptr;

    if (fullPath.empty()) {
        result = m_RootDir;
    } else {
        auto it = std::find_if(m_Directories.begin(), m_Directories.end(), [=](const Directory* dir)->bool {
            return dir->fullPath == fullPath;
        });

        if (it != m_Directories.end()) {
            result = *it;
        }
    }

    return result;
}

bool MainWindow::ExtractFile(const qc::u64 idx, const QString& dstFileName) {
    bool result = false;

    auto data = m_PAK->GetFileUncompressedData(idx);
    QFile file(dstFileName);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        file.flush();
        file.close();
        result = true;
    }

    return result;
}

bool MainWindow::ExtractDirectory(const Directory* dir, const QString& dstPath) {
    bool result = false;

    qc::String parentDir = dstPath.toStdString();
    if (parentDir.back() != '/') {
        parentDir.push_back('/');
    }

    if (dir && dstPath.length() > 1) {
        for (auto f : dir->files) {
            qc::String fileName = fs::path(m_PAK->GetFileName(f)).filename().string();
            if (!this->ExtractFile(f, QString::fromStdString(parentDir + fileName))) {
                return false;
            }
        }

        for (auto d : dir->directories) {
            qc::String newDir = parentDir + d->name;
            ::CreateDirectoryA(newDir.c_str(), nullptr);
            if (!this->ExtractDirectory(d, QString::fromStdString(newDir))) {
                result = false;
                break;
            }
        }

        result = true;
    }

    return result;
}


void MainWindow::on_action_Open_triggered() {
    if (m_PAK) {
        delete m_PAK;
    }

    QString path = QFileDialog::getOpenFileName(this, "Select Quake Champions pak...", QString(), tr("Quake Champions pak (*.pak)"));
    if (path.length() > 3) {
        m_PAK = new qc::PAKArchive();
        if (m_PAK->LoadFromFile(path.toStdString())) {
            this->BuildDirectories();
            this->UpdateDirectoriesTree();
        } else {
            delete m_PAK;
            m_PAK = nullptr;
        }
    }
}

void MainWindow::on_treeWidget_itemSelectionChanged() {
    auto items = ui->treeWidget->selectedItems();
    if (items.size()) {
        auto item = items[0];
        QVariant var = item->data(0, Qt::UserRole);
        Directory* dir = var.value<Directory*>();
        if (dir) {
            this->UpdateFilesTable(dir);
        }
    }
}

void MainWindow::on_filesList_itemSelectionChanged() {

}

void MainWindow::on_treeWidget_customContextMenuRequested(const QPoint& pos) {
    QMenu* menu = new QMenu(ui->filesList);
    menu->addAction(tr("Extract folder..."), this, SLOT(on_action_ExtractFolder_triggered()));
    menu->exec(ui->treeWidget->mapToGlobal(pos));
}

void MainWindow::on_filesList_customContextMenuRequested(const QPoint& pos) {
    QMenu* menu = new QMenu(ui->filesList);
//        menu->addAction(QIcon(":/resources/icons/Save_24x24.png"), tr("Extract resource (%1) ...").arg(GetFileTypeString(info.fileType)), this, SLOT(on_actionExtract_selected_triggered()));
//        menu->addSeparator();
//        menu->addAction(QIcon(":/resources/icons/Information_48x48.png"), tr("Show file information..."), this, SLOT(on_actionShow_file_information_triggered()));
    menu->addAction(tr("Extract file..."), this, SLOT(on_action_ExtractFile_triggered()));
    menu->exec(ui->filesList->mapToGlobal(pos));
}

void MainWindow::on_action_ExtractFile_triggered() {
    auto items = ui->filesList->selectedItems();
    if (items.size()) {
        auto item = items[0];
        QVariant var = item->data(0, Qt::UserRole);
        const qc::u64 fileIdx = var.value<qc::u64>();
        const qc::String& fullName = m_PAK->GetFileName(fileIdx);
        auto pos = fullName.find_last_of('\\');
        qc::String name = fullName.substr(pos + 1);

        QString savePath = QFileDialog::getSaveFileName(this, tr("Where to save..."), QString::fromStdString(name), tr("All files (*.*)"));
        if (savePath.length() > 3) {
            bool result = this->ExtractFile(fileIdx, savePath);
            if (result) {
                QMessageBox::information(this, tr("QCPAK"), tr("File extraction succeeded!"));
            } else {
                QMessageBox::critical(this, tr("QCPAK"), tr("File extraction failed!"));
            }
        }
    }
}

void MainWindow::on_action_ExtractFolder_triggered() {
    auto items = ui->treeWidget->selectedItems();
    if (items.size()) {
        auto item = items[0];
        QVariant var = item->data(0, Qt::UserRole);
        Directory* dir = var.value<Directory*>();
        if (dir) {
            QString savePath = QFileDialog::getExistingDirectory(this, tr("Where to save..."), QString(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
            if (savePath.length() > 3) {
                QProgressDialog processDlg(this);
                processDlg.setWindowModality(Qt::WindowModal);
                processDlg.show();
                bool result = this->ExtractDirectory(dir, savePath);
            }
        }
    }
}

void MainWindow::on_treeWidget_itemCollapsed(QTreeWidgetItem* item) {
    item->setIcon(0, QIcon(":/icons/folder_closed.png"));
}

void MainWindow::on_treeWidget_itemExpanded(QTreeWidgetItem* item) {
    item->setIcon(0, QIcon(":/icons/folder_opened.png"));
}

void MainWindow::on_actionAbout_triggered() {
    AboutDlg about(this);
    about.exec();
}

void MainWindow::on_actionAbout_Qt_triggered() {
    QMessageBox::aboutQt(this, "About Qt");
}
