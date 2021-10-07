#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeWidget>

#include "qc/PAKArchive.h"

#include <list>

namespace Ui {
class MainWindow;
}

struct Directory {
    static const char DELIMITER = '\\';

    qc::String              name;
    qc::String              fullPath;
    std::list<Directory*>   directories;
    qc::Array<qc::u64>      files;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private:
    void        BuildDirectories();
    void        AddDirRecursively(Directory* dir, QTreeWidgetItem* parent);
    void        UpdateDirectoriesTree();
    void        UpdateFilesTable(Directory* dir);
    Directory*  GetOrCreateDirectory(const qc::String& fullPath);
    Directory*  FindDirectory(const qc::String& fullPath);

    bool        ExtractFile(const qc::u64 idx, const QString& dstFileName);
    bool        ExtractDirectory(const Directory* dir, const QString& dstPath);

private slots:
    void on_action_Open_triggered();
    void on_treeWidget_itemSelectionChanged();
    void on_filesList_itemSelectionChanged();
    void on_treeWidget_customContextMenuRequested(const QPoint &pos);
    void on_filesList_customContextMenuRequested(const QPoint &pos);
    void on_action_ExtractFile_triggered();
    void on_action_ExtractFolder_triggered();
    void on_treeWidget_itemCollapsed(QTreeWidgetItem *item);
    void on_treeWidget_itemExpanded(QTreeWidgetItem *item);
    void on_actionAbout_triggered();
    void on_actionAbout_Qt_triggered();

private:
    Ui::MainWindow*         ui;
    qc::PAKArchive*         m_PAK;
    Directory*              m_RootDir;
    std::vector<Directory*> m_Directories;
};

#endif // MAINWINDOW_H
