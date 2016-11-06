/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

/*
    treemodel.cpp

    Provides a simple tree model to show how to create and use hierarchical
    models.
*/

#include <QColor>
#include <QFont>
#include <QStringList>
#include <time.h>
#include <map>
#include <vector>

#include "../contrib/xxhash/xxhash.h"
#include "../libuat/Archive.hpp"
#include "../libuat/PersistentStorage.hpp"
#include "browser.h"
#include "treeitem.hpp"
#include "treemodel.hpp"

TreeModel::TreeModel(const Archive &data, PersistentStorage& storage, Browser* browser, QObject *parent)
    : QAbstractItemModel(parent)
    , m_indices( data.NumberOfMessages() )
    , m_archive( data )
    , m_storage( storage )
    , m_browser( browser )
{
    QVector<QVariant> rootData;
    rootData << "Subject" << "Posts" << "Author" << "Date";
    rootItem = new TreeItem();
    rootItem->setData( std::move( rootData ), 0 );
    setupModelData(rootItem);
}

TreeModel::~TreeModel()
{
    delete rootItem;
}

int TreeModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return static_cast<TreeItem*>(parent.internalPointer())->columnCount();
    else
        return rootItem->columnCount();
}

bool TreeModel::canFetchMore( const QModelIndex& parent ) const
{
    if( !parent.isValid() ) return false;
    TreeItem *item = static_cast<TreeItem*>(parent.internalPointer());
    if( item->m_wasExpanded ) return false;
    const auto idx = item->GetIdx();
    return m_archive.GetParent( idx ) == -1 && m_archive.GetTotalChildrenCount( idx ) > 1;
}

bool TreeModel::hasChildren( const QModelIndex& parent ) const
{
    if( !parent.isValid() ) return true;
    TreeItem *item = static_cast<TreeItem*>(parent.internalPointer());
    const auto idx = item->GetIdx();
    return m_archive.GetTotalChildrenCount( idx ) > 1;
}

void TreeModel::fetchMore( const QModelIndex& _parent )
{
    TreeItem* parent = static_cast<TreeItem*>(_parent.internalPointer());
    if( parent->m_wasExpanded ) return;
    parent->m_wasExpanded = true;
    auto top = m_archive.GetChildren( parent->GetIdx() );
    std::map<const char*, uint8_t> nameColor;
    std::vector<int32_t> items;
    std::vector<TreeItem*> parents = { parent };
    items.reserve( top.size * 2 );
    for( int i = top.size-1; i >= 0; i-- )
    {
        items.emplace_back( top.ptr[i] );
    }
    while( !items.empty() )
    {
        auto idx = items.back();
        items.pop_back();
        if( idx == -1 )
        {
            parents.pop_back();
        }
        else
        {
            parent = parents.back();
            auto item = new TreeItem( parent, idx );
            parent->appendChild( item );

            uint8_t color;
            auto fromptr = m_archive.GetFrom( idx );
            auto it = nameColor.find( fromptr );
            if( it == nameColor.end() )
            {
                auto hash = XXH32( fromptr, strlen( fromptr ), 0 );
                color = hash & 0xFF;
                nameColor.emplace( fromptr, color );
            }
            else
            {
                color = it->second;
            }

            QVector<QVariant> columns;
            columns << m_archive.GetSubject( idx );
            columns << QString::number( m_archive.GetTotalChildrenCount( idx ) );
            columns << m_archive.GetRealName( idx );
            auto date = m_archive.GetDate( idx );
            time_t t = { date };
            char* tmp = asctime( localtime( &t ) );
            tmp[strlen(tmp)-1] = '\0';
            columns << tmp;
            item->setData( std::move( columns ), color );

            const auto children = m_archive.GetChildren( idx );
            if( children.size > 0 )
            {
                parents.emplace_back( item );
                items.emplace_back( -1 );
                for( int i = children.size-1; i >= 0; i-- )
                {
                    items.emplace_back( children.ptr[i] );
                }
            }
        }
    }
    m_browser->RecursiveSetIndex( _parent );
}

QVariant TreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    TreeItem *item = static_cast<TreeItem*>(index.internalPointer());

    switch( role )
    {
    case Qt::DisplayRole:
        return item->data(index.column());
    case Qt::ForegroundRole:
        switch( index.column() )
        {
        case 0:
            if( m_storage.WasVisited( m_archive.GetMessageId( item->GetIdx() ) ) )
            {
                bool complete = true;
                std::vector<uint32_t> stack;
                stack.reserve( 4 * 1024 );
                stack.emplace_back( item->GetIdx() );
                while( !stack.empty() )
                {
                    const auto idx = stack.back();
                    if( !m_storage.WasVisited( m_archive.GetMessageId( idx ) ) )
                    {
                        complete = false;
                        break;
                    }
                    stack.pop_back();
                    const auto children = m_archive.GetChildren( idx );
                    for( int i=0; i<children.size; i++ )
                    {
                        stack.emplace_back( children.ptr[i] );
                    }
                }

                QColor c;
                if( complete )
                {
                    c.setRgb( 96, 96, 96 );
                }
                else
                {
                    c.setRgb( 128, 128, 128 );
                }
                return QVariant( c );
            }
            break;
        case 2:
            {
            QColor c;
            c.setHsv( item->GetColor(), 160, 180 );
            return QVariant( c );
            }
            break;
        default:
            return QVariant();
        }
    default:
        return QVariant();
    }
}

uint32_t TreeModel::GetIdx(const QModelIndex& index) const
{
    if (!index.isValid())
        return -1;

    TreeItem *item = static_cast<TreeItem*>(index.internalPointer());
    return item->GetIdx();
}

bool TreeModel::IsRoot(const QModelIndex& index) const
{
    if (!index.isValid())
        return false;

    TreeItem *childItem = static_cast<TreeItem*>(index.internalPointer());
    TreeItem *parentItem = childItem->parentItem();

    return parentItem == rootItem;
}

Qt::ItemFlags TreeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;

    return QAbstractItemModel::flags(index);
}

QVariant TreeModel::headerData(int section, Qt::Orientation orientation,
                               int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return rootItem->data(section);

    return QVariant();
}

QModelIndex TreeModel::index(int row, int column, const QModelIndex &parent)
            const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    TreeItem *parentItem;

    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = static_cast<TreeItem*>(parent.internalPointer());

    TreeItem *childItem = parentItem->child(row);
    if (childItem)
        return createIndex(row, column, childItem);
    else
        return QModelIndex();
}

QModelIndex TreeModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    TreeItem *childItem = static_cast<TreeItem*>(index.internalPointer());
    TreeItem *parentItem = childItem->parentItem();

    if (parentItem == rootItem)
        return QModelIndex();

    return createIndex(parentItem->row(), 0, parentItem);
}

int TreeModel::rowCount(const QModelIndex &parent) const
{
    TreeItem *parentItem;
    if (parent.column() > 0)
        return 0;

    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = static_cast<TreeItem*>(parent.internalPointer());

    return parentItem->childCount();
}

void TreeModel::setupModelData( TreeItem* parent )
{
    auto top = m_archive.GetTopLevel();
    std::map<const char*, uint8_t> nameColor;
    for( int i = 0; i<top.size; i++ )
    {
        const auto& idx = top.ptr[i];

        auto item = new TreeItem( parent, idx );
        parent->appendChild( item );

        uint8_t color;
        auto fromptr = m_archive.GetFrom( idx );
        auto it = nameColor.find( fromptr );
        if( it == nameColor.end() )
        {
            auto hash = XXH32( fromptr, strlen( fromptr ), 0 );
            color = hash & 0xFF;
            nameColor.emplace( fromptr, color );
        }
        else
        {
            color = it->second;
        }

        QVector<QVariant> columns;
        columns << m_archive.GetSubject( idx );
        columns << QString::number( m_archive.GetTotalChildrenCount( idx ) );
        columns << m_archive.GetRealName( idx );
        auto date = m_archive.GetDate( idx );
        time_t t = { date };
        char* tmp = asctime( localtime( &t ) );
        tmp[strlen(tmp)-1] = '\0';
        columns << tmp;
        item->setData( std::move( columns ), color );
    }
}

QModelIndex TreeModel::GetIndexFor( uint32_t idx ) const
{
    return m_indices[idx];
}
