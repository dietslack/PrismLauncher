// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QAbstractTableModel>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QWidget>

#include <memory>
#include <vector>

class MinecraftInstance;
class QButtonGroup;
class QComboBox;
class QLabel;
class QLineEdit;
class QTreeView;
class QWidget;
class Resource;
class ResourceFolderModel;

/// One table over all of an instance's content: mods, resource packs, data packs and shaders.
/// Rows can be checked (selected) for bulk actions; checks are kept by file path across reloads.
class ContentModel : public QAbstractTableModel {
    Q_OBJECT
   public:
    enum class Type { Mod, ResourcePack, DataPack, Shader };
    enum Column { SelectColumn, NameColumn, VersionColumn, TypeColumn, SourceColumn, EnabledColumn, ColumnCount };
    enum Roles { TypeRole = Qt::UserRole + 1, FileNameRole, DateRole, EnabledRole };

    explicit ContentModel(QObject* parent = nullptr);
    void setInstance(MinecraftInstance* instance);
    void reload();

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    /// The source folder model and its index for `row`.
    std::pair<ResourceFolderModel*, QModelIndex> source(int row) const;
    Resource* resource(int row) const;
    ResourceFolderModel* folder(Type type) const;
    int count(Type type) const;
    static QString typeName(Type type);

    bool isChecked(int row) const;
    void setChecked(const QList<int>& rows, bool checked);
    QList<int> checkedRows() const;

   signals:
    void checksChanged();

   private:
    void rebuild();
    QString pathOf(int row) const;

    struct Entry {
        ResourceFolderModel* model;
        int row;
        Type type;
    };
    QList<std::pair<ResourceFolderModel*, Type>> m_folders;
    QHash<ResourceFolderModel*, QString> m_worldNames;               ///< world each per-world data pack folder belongs to
    std::vector<std::unique_ptr<ResourceFolderModel>> m_ownedFolders;  ///< per-world data pack folders
    QList<Entry> m_entries;
    QSet<QString> m_checked;  ///< absolute file paths of checked rows
};

class ContentFilterProxy : public QSortFilterProxyModel {
    Q_OBJECT
   public:
    using QSortFilterProxyModel::QSortFilterProxyModel;
    void setTypeFilter(int type);  ///< -1 shows every type
    void setSearch(const QString& text);

   protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

   private:
    int m_type = -1;
    QString m_search;
};

/// "Content" tab of the instance page.
class ContentTab : public QWidget {
    Q_OBJECT
   public:
    explicit ContentTab(QWidget* parent = nullptr);
    void setInstance(MinecraftInstance* instance);

   private:
    void updateCounts();
    void applySort(int option);
    void showContextMenu(const QPoint& pos);
    void addFiles(ContentModel::Type type);
    void setEnabled(const QList<int>& rows, bool enabled);
    void deleteRows(const QList<int>& rows);
    QList<int> visibleRows() const;

    MinecraftInstance* m_instance = nullptr;
    ContentModel* m_model;
    ContentFilterProxy* m_proxy;
    QTreeView* m_view;
    QLineEdit* m_search;
    QComboBox* m_sort;
    QButtonGroup* m_filters;
    QLabel* m_summary;
    QWidget* m_bulkBar;
    QLabel* m_bulkLabel;
};
