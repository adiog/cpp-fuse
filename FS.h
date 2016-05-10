// This file is a part of cpp-fuse project.
// Copyright (c) 2016 Aleksander Gajewski <adiog@brainfuck.pl>.

#ifndef CPP_FUSE_FS_H
#define CPP_FUSE_FS_H

#include <memory>
#include <unordered_map>
#include "util/split"
#include "util/throw"

class FSNode {
  public:
    template <typename String>
    FSNode(String &&name) : name(std::forward<String>(name)) {}

    const std::string &getName(void) const { return name; }

    virtual bool isDir(void) const { return false; }
    virtual bool isFile(void) const { return false; }
    virtual bool isLink(void) const { return false; }

  protected:
    std::string name;
};

class FSDirNode : public FSNode {
  public:
    template <typename String>
    FSDirNode(String &&name) : FSNode(std::forward<String>(name)) {}

    virtual bool isDir(void) const override { return true; }

    size_t getDirSize(void) const { return content.size(); }

    std::shared_ptr<FSNode> getNode(const std::string &name) const {
        auto node = content.find(name);
        if (node != content.end()) {
            return node->second;
        } else {
            return nullptr;
        }
    }

    void addNode(std::shared_ptr<FSNode> node) {
        if (!hasNodeByName(node->getName())) {
            content[node->getName()] = node;
        } else {
            THROW();
        }
    }

    bool hasNodeByName(const std::string &name) const {
        return (content.find(name) != content.end());
    }

    std::vector<std::string> getDir(void) const {
        std::vector<std::string> directory;
        directory.reserve(content.size());
        for (const auto &node : content) {
            directory.push_back(node.second->getName());
        }
        return directory;
    }

  private:
    std::unordered_map<std::string, std::shared_ptr<FSNode>> content;
};

class FSLinkNode : public FSNode {
  public:
    template <typename S1, typename S2>
    FSLinkNode(S1 &&name, S2 &&link)
        : FSNode(std::forward<S1>(name)), link(std::forward<S2>(link)) {}

    virtual bool isLink(void) const override { return true; }

    size_t getLinkSize(void) const { return link.size(); }
    size_t getLink(char *buf, size_t size) const {
        if (size > link.size()) {
            std::memcpy(buf, &link[0], link.size());
            buf[link.size()] = '\0';
        } else {
            THROW();
        }
        return link.size();
    }

  private:
    std::string link;
};

class FSFileNode : public FSNode {
  public:
    template <typename S1, typename S2>
    FSFileNode(S1 &&name, S2 &&content)
        : FSNode(std::forward<S1>(name)), content(std::forward<S2>(content)) {}

    virtual bool isFile(void) const override { return true; }

    size_t getFileSize(void) const { return content.size(); }
    size_t getData(char *buf, off_t offset, size_t size) const {
        if (static_cast<size_t>(offset) > content.size()) {
            size = 0U;
        } else if (content.size() < static_cast<size_t>(offset) + size) {
            size = content.size() - offset;
        }

        std::memcpy(buf, &content[offset], size);

        return size;
    }

  private:
    std::string content;
};

class FS {
  public:
    FS() { root = std::make_shared<FSDirNode>(std::string("/")); }

    void init() {
        // currently not used
    }

    std::shared_ptr<FSDirNode> make_dir(const char *path) {
        return make_dir(std::string(path));
    }

    template <typename String>
    std::shared_ptr<FSDirNode> make_dir(String &&path) {
        std::string path_to_split = (path[0] == '/')
                                        ? std::string(&path[1], path.size() - 1)
                                        : std::forward<String>(path);
        std::vector<std::string> path_split = util::split(path_to_split, '/');
        std::shared_ptr<FSDirNode> dirNode = root;
        for (auto &name : path_split) {
            std::shared_ptr<FSNode> son = dirNode->getNode(name);
            if (!son) {
                son = std::dynamic_pointer_cast<FSNode>(
                    std::make_shared<FSDirNode>(name));
                dirNode->addNode(son);
                dirNode = std::dynamic_pointer_cast<FSDirNode>(son);
            } else {
                if (son->isDir()) {
                    dirNode = std::dynamic_pointer_cast<FSDirNode>(son);
                } else {
                    THROW();
                }
            }
        }
        return dirNode;
    }

    template <typename S1, typename S2, typename S3>
    void make_link(S1 &&path, S2 &&name, S3 &&ospath) {
        std::shared_ptr<FSNode> linkNode = std::make_shared<FSLinkNode>(
            std::forward<S2>(name), std::forward<S3>(ospath));
        auto parent = make_dir(std::forward<S1>(path));
        if (parent) {
            parent->addNode(linkNode);
        }
    }

    template <typename S1, typename S2, typename S3>
    void make_file(S1 &&path, S2 &&name, S3 &&content) {
        std::shared_ptr<FSNode> fileNode = std::make_shared<FSFileNode>(
            std::forward<S2>(name), std::forward<S3>(content));
        auto parent = make_dir(std::forward<S1>(path));
        if (parent) {
            parent->addNode(fileNode);
        }
    }

    std::shared_ptr<FSNode> getNode(const char *path) {
        std::string path_to_split = (path[0] == '/') ? path + 1 : path;
        std::vector<std::string> path_split = util::split(path_to_split, '/');
        std::shared_ptr<FSNode> node = root;
        for (auto &name : path_split) {
            if (!node->isDir()) {
                return nullptr;
            } else {
                node =
                    std::dynamic_pointer_cast<FSDirNode>(node)->getNode(name);
                if (!node) {
                    return nullptr;
                }
            }
        }
        return node;
    }

  public:
    bool isNode(const char *path) { return (getNode(path) != nullptr); }

    bool isDir(const char *path) {
        std::shared_ptr<FSNode> node = getNode(path);
        return ((node) && (node->isDir()));
    }

    bool isLink(const char *path) {
        std::shared_ptr<FSNode> node = getNode(path);
        return ((node) && (node->isLink()));
    }

    bool isFile(const char *path) {
        std::shared_ptr<FSNode> node = getNode(path);
        return ((node) && (node->isFile()));
    }

    size_t getDirSize(const char *path) {
        std::shared_ptr<FSNode> node = getNode(path);
        if ((node) && (node->isDir())) {
            auto dirNode = std::dynamic_pointer_cast<FSDirNode>(node);
            return dirNode->getDirSize();
        } else {
            THROW();
        }
    }

    size_t getLinkSize(const char *path) {
        std::shared_ptr<FSNode> node = getNode(path);
        if ((node) && (node->isLink())) {
            auto linkNode = std::dynamic_pointer_cast<FSLinkNode>(node);
            return linkNode->getLinkSize();
        } else {
            THROW();
        }
    }

    size_t getFileSize(const char *path) {
        std::shared_ptr<FSNode> node = getNode(path);
        if ((node) && (node->isFile())) {
            auto fileNode = std::dynamic_pointer_cast<FSFileNode>(node);
            return fileNode->getFileSize();
        } else {
            THROW();
        }
    }

    std::vector<std::string> getDir(const char *path) {
        std::shared_ptr<FSNode> node = getNode(path);
        if ((node) && (node->isDir())) {
            auto dirNode = std::dynamic_pointer_cast<FSDirNode>(node);
            return dirNode->getDir();
        } else {
            THROW();
        }
    }

    size_t readFile(const char *path, char *buf, size_t size, off_t offset) {
        std::shared_ptr<FSNode> node = getNode(path);
        if ((node) && (node->isFile())) {
            auto fileNode = std::dynamic_pointer_cast<FSFileNode>(node);
            size_t length = fileNode->getFileSize();
            if (static_cast<size_t>(offset) < length) {
                if (offset + size > length) {
                    size = length - offset;
                }
                size = fileNode->getData(buf, offset, size);
            } else {
                size = 0U;
            }
        } else {
            THROW();
        }
        return size;
    }

    size_t readLink(const char *path, char *buf, size_t size) {
        std::shared_ptr<FSNode> node = getNode(path);
        if ((node) && (node->isLink())) {
            auto linkNode = std::dynamic_pointer_cast<FSLinkNode>(node);
            size = linkNode->getLink(buf, size);
        } else {
            THROW();
        }
        return size;
    }

    const std::string root_path = "/";

  private:
    std::shared_ptr<FSDirNode> root;
};

#endif  // CPP_FUSE_FS_H
