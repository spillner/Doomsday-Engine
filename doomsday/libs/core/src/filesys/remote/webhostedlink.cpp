/** @file
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/filesys/WebHostedLink"

#include "de/Async"
#include "de/Folder"
#include "de/PathTree"
#include "de/RecordValue"
#include "de/RemoteFeedRelay"
#include "de/TextValue"
#include "de/WebRequest"

namespace de {
namespace filesys {

DE_PIMPL(WebHostedLink)
, public Lockable
{
    Set<WebRequest *>         pendingRequests;
    std::shared_ptr<FileTree> fileTree;

    Impl(Public *i) : Base(i)
    {}

    Block metaIdForFileEntry(const FileEntry &entry) const
    {
        if (entry.isBranch()) return {}; // not applicable
        return md5Hash(self().address(), entry.path(), entry.size, entry.modTime);
    }

    void handleFileListQueryAsync(Query query)
    {
        QueryId const id = query.id;
        String const queryPath = query.path;
        self().scope() += async([this, queryPath] () -> std::shared_ptr<DictionaryValue>
        {
            DE_GUARD(this);
            if (auto const *dir = fileTree->tryFind
                    (queryPath, FileTree::MatchFull | FileTree::NoLeaf))
            {
                std::shared_ptr<DictionaryValue> list(new DictionaryValue);

                static const String VAR_TYPE       ("type");
                static const String VAR_MODIFIED_AT("modifiedAt");
                static const String VAR_SIZE       ("size");
                static const String VAR_META_ID    ("metaId");

                auto addMeta = [this](DictionaryValue &list, const PathTree::Nodes &nodes) {
                    for (const auto &i : nodes)
                    {
                        auto const &entry = i.second->as<FileEntry>();
                        list.add(new TextValue(entry.name()),
                                  RecordValue::takeRecord(
                                      Record::withMembers(
                                          VAR_TYPE,        entry.isLeaf()? 0 : 1,
                                          VAR_SIZE,        entry.size,
                                          VAR_MODIFIED_AT, entry.modTime,
                                          VAR_META_ID,     metaIdForFileEntry(entry)
                                      )));
                    }
                };

                addMeta(*list.get(), dir->children().branches);
                addMeta(*list.get(), dir->children().leaves);
                return list;
            }
            return nullptr;
        },
        [this, id](std::shared_ptr<DictionaryValue> list) {
            self().metadataReceived(id, list? *list : DictionaryValue());
        });
    }

    void receiveFileContents(QueryId id, WebRequest &web)
    {
        if (web.isFailed())
        {
            LOG_NET_WARNING(web.errorMessage());
            /// @todo Abort query with error.
            return;
        }
//        if (web.isPending())
//        {
//            return;
//        }
//        if (web.isSucceeded())
//        {
        //qDebug() << "Content-Length:" << reply->header(QNetworkRequest::ContentLengthHeader);
        const dsize contentLength = web.contentLength(); //reply->header(QNetworkRequest::ContentLengthHeader).toULongLong();

        //qDebug() << "pos:" << pos << contentLength << reply->url();

        // Ths is the complete downloaded file.
        //            QByteArray const data = reply->readAll();
        const Block data = web.readAll();

        Query const *query = self().findQuery(id);
        self().chunkReceived(id, query->receivedBytes, data,
                             contentLength ? contentLength : dsize(data.size()));
        //        }
    }
};

WebHostedLink::WebHostedLink(String const &address, String const &indexPath)
    : Link(address)
    , d(new Impl(this))
{
    // Fetch the repository index.
    {
        WebRequest *req = new WebRequest;
        req->setUserAgent(Version::currentBuild().userAgent());
        req->audienceForFinished() += [this, req]() {
            trash(req);
            if (req->isSucceeded())
            {
                parseRepositoryIndex(req->result());
            }
            else
            {
                handleError(req->errorMessage());
                wasDisconnected();
            }
        };
        req->get(address / indexPath);


//        QNetworkReply *reply = filesys::RemoteFeedRelay::get().network().get(req);
//        QObject::connect(reply, &QNetworkReply::finished, [this, reply] ()
//        {
//        });
    }
}

void WebHostedLink::setFileTree(FileTree *tree)
{
    DE_GUARD(d);
    d->fileTree.reset(tree);
}

WebHostedLink::FileTree const &WebHostedLink::fileTree() const
{
    return *d->fileTree;
}

WebHostedLink::FileEntry const *WebHostedLink::findFile(Path const &path) const
{
    DE_GUARD(d);
    return d->fileTree->tryFind(path, PathTree::MatchFull);
}

filesys::PackagePaths WebHostedLink::locatePackages(StringList const &packageIds) const
{
    PackagePaths remotePaths;
    for (const String &packageId : packageIds)
    {
        if (String remotePath = findPackagePath(packageId))
        {
            remotePaths.insert(packageId,
                               RepositoryPath(*this, localRoot().path() / packageId, remotePath));
        }
    }
    return remotePaths;
}

void WebHostedLink::transmit(Query const &query)
{
    // We can answer population queries instantly because an index was
    // downloaded when the connection was opened.
    if (query.fileMetadata)
    {
        d->handleFileListQueryAsync(query);
        return;
    }

    DE_ASSERT(query.fileContents);

    String uri = address().concatenateRelativePath(query.path);
    //QNetworkRequest req(url);
    //qDebug() << req.url().toString();
    debug("[WebHostedLink] Get URL: %s", uri.c_str());
    //req.setRawHeader("User-Agent", Version::currentBuild().userAgent().toLatin1());
    WebRequest *web = new WebRequest;
    web->setUserAgent(Version::currentBuild().userAgent());

    //QNetworkReply *reply = RemoteFeedRelay::get().network().get(req);
    d->pendingRequests.insert(web);

    const auto id = query.id;
    web->audienceForProgress() += [this, id, web]() {
        d->receiveFileContents(id, *web);
    };
    web->audienceForFinished() += [this, web]() {
        d->pendingRequests.remove(web);
        trash(web);
    };
    web->get(uri);
}

Block WebHostedLink::FileEntry::metaId(Link const &link) const
{
    return md5Hash(link.address(), path(), size, modTime);
}

} // namespace filesys
} // namespace de
