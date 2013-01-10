/**
 *  \file design.h
 *
 *  \brief ZlomekFS design details
 *  \author based on Josef Zlomek thesis
 */

/*!
   \page zfs-design ZlomekFS design details

   \tableofcontents

   This page describes the design of the ZFS file system from the high-level
   view. Alternatives are discussed where appropriate, and the reasons are given
   for why one of them was chosen while the others were not.

   \section volumes Volumes

   The file system consists of volumes. A volume is a directory tree that is
   accessible by other nodes and may be cached by them. If the volume is
   provided by the node or is cached by the node, it is located in a directory
   in the local file system of the node. The volume must be placed in one local
   file system so that it is possible to link or move files between any two parts
   of the volume.

   \subsection volume_mount_points Volume Mount Points

   The volumes are mounted to one directory tree, which gets mounted to node's
   file system hierarchy at a time. There are two possibilities where the volumes
   could be mounted.

   The first option is to mount each volume right under the root of the file
   system as a directory whose name is the name of the volume. This is simple
   and does not need special data structures to describe the mount points except
   the list of volumes. But this is a rather limiting solution.

   The solution used in ZFS is more flexible. Each volume may be mounted
   in any path from the root of the file system. The parts of the path to the
   volume mount point need to be represented by virtual directories. The user
   cannot do any modifying operations in the virtual directories. If volume
   X is mounted under a mount point of volume Y, the files or directories of
   volume Y that have the same names as the corresponding virtual directories
   are overshadowed by these virtual directories and cannot be accessed.

   The mount points
   of volumes are the virtual directories config, corba, ZFS, volX and volY. If
   there is a file or directory more in root of the volume with the mount point
   volX, it is overshadowed by the virtual directory more.

   Another question is whether all volume mount points or just the mount
   points of volumes provided by connected nodes should be visible in the file
   system.

   If volume mount points of volumes provided by disconnected nodes were
   not visible, it would not be possible to make the node connect to the corre-
   sponding volume provider by doing any file system operation. Therefore, the
   file system would have to provide a special tool that would cause the node
   to connect, but this would not be a good solution.

   So the whole virtual directory tree is visible in the file system. When
   the local node wants to make a file system operation with the volume mount
   point and the node providing the volume is disconnected, the local node tries
   to connect to that node. If the volume provider is online, the connection is
   established and the operation is performed on the volume root, otherwise
   just the virtual directory is accessed.

   \subsection volume_hierarchy   Volume Hierarchy

   The model with a primary location of the volume and other nodes caching the
   volume or accessing it remotely, which was chosen in the previous chapter,
   can be easily extended as follows. Each node may choose whether it wants to
   access the volume by talking to the node providing the volume or to another
   node, such a node is called a volume master. Although the node's volume
   master may be any node, it should be a node that provides or caches the
   volume. It does not make sense for a node that does not cache the volume
   to be a volume master of other nodes.
       The volume master relation forms a volume hierarchy. The root of the
   hierarchy is the node that provides the volume, i.e. the node that holds the
   primary replica. The hierarchy may, of course, be different for each volume.
   The benefit of the hierarchy is that all nodes do not interact with the volume
   provider to update their caches or to access the volume remotely, so the
   workload is handled by multiple nodes.

   \verbatim
       Figure 3.2: Volume Hierarchy

   			      N1



   	 N2                   N3                      N7


   		    N4        N5        N6            N8        N9

   \endverbatim



   \section accessing_the_volumes Accessing the Volumes

   There are three distinct types of volumes from the view of a node - volumes
   provided by the node, volumes accessed remotely and volumes cached on the
   local disk. These types are accessed differently by the node as described in
   the following sections.

   \subsection volumes_provided_by_the_node Volumes Provided by the Node

   A node may provide several volumes to the other nodes. It means that
   the primary replica of each such a volume on the node's local disk. When
   the node is reading or modifying files or directories on such a volume, the
   file system is accessing the appropriate files or directories in the local file
   system. It also updates its additional metadata when modifying operations
   are performed. Although the files of the volume are stored in the local file
   system, it is not a good idea to change the files via the local file system
   because the metadata would not be updated and thus the nodes that cache
   the volume would not detect the changes.

   \subsection volumes_accessed_remotely Volumes Accessed Remotely

   The node does not hold any data of such volumes on its disk. All the files
   and directories of these volumes are accessed remotely. Every file system
   operation is invoked on the node's volume master using the remote procedure
   call mechanism. Therefore, no problems with synchronizing the files between
   the local node and the volume master arise. If the node cannot connect to
   the volume master, an error is returned to the user.

   The remote access to the volume is supposed to be used when the volume
   contains files that are often accessed by at least two nodes at the same time
   while at least one node is modifying them, or when the node does not have
   enough space on its disk to cache the volume, or when the node simply does
   not want to cache the volume.

   \subsection volumes_cached_by_the_node Volumes Cached by the Node

   For each volume of this type, a cache is maintained in the local file system
   of the node. This cache contains a subset of volume's directories and files.
   The cached volumes support disconnected operation.

   The cached volume could be accessed in two different ways.
   The first option is to use the local cache only when the node cannot
   connect to its volume master, and otherwise to access the volume remotely
   and update the local cache for the case the node gets disconnected. However,
   this would require special handling according to the connection status.
   The local cache would not improve performance and would be good only for
   disconnected operation.

   The option used in ZFS is to always access the local cache and try to
   synchronize the corresponding file or directory before doing the requested
   file system operation. This would increase the performance
   and decrease the network trafic compared to the first option. The connected
   and disconnected operations differ only in the fact that the local cache is not
   synchronized when the node is disconnected. Therefore, no special handling
   of these two cases is required.

   Both of these options require the metadata to be updated so that it
   would be possible to detect whether the file has changed, and to maintain
   a modification log so that the changes made to the local
   cache could be reintegrated. Similarly to the volumes provided by the node,
   it is not a good idea to change the cached files via the local file system.

   The caching should be used for the volumes that contain files that do not
   change too often and the user wants to use those volumes while the node is
   disconnected.


   \section access_rights Access rights

   Every file system should provide a way to specify the access rights of files so
   that the read and write access can be limited to a subset of users.

   The easiest way to specify the access rights in Unix environment is to
   grant rights to the owner of the file, to the group of the owner and to the
   others where the owner and the group is a system user or group, respectively.
   The user and group IDs have to be the same on all nodes accessing the file
   system, but this is not the common case. On the other hand, this solution
   does not need any additional information because the access rights are stored
   in the local file system.

   A better option is to define file system users and groups, specify a map-
   ping between the node's users and groups and the file system ones, and
   describe the access rights similar to the previous case. This requires the list
   of users and groups and the mapping for each node to be explicitly stored,
   for example, in the configuration of the file system. This solution is currently
   used in ZFS because it is still simple but more flexible than the previous one.

   The best solution would be to use the Access Control Lists (ACLs), which
   enable to specify a list of entities (users or groups) that are allowed to access
   a given file. The access rights are attached separately to each entity in the
   list. However, it is more complicated to store the ACL in the file system
   because it has a variable length, and to check the permission using the ACL.
   Moreover, the users have to be authenticated to the file system to increase the
   security, for example by Kerberos. According to [1], the ACLs and Kerberos
   authentication are used by the file systems that have powerful security, for
   example AFS and Coda. The use of ACLs is a good instance
   of possible future improvements of ZFS.

   \section synchronizing_the_Cached_volumes  Synchronizing the Cached Volumes

   When a volume is cached on a node, it is necessary to synchronize its contents
   with the node's volume master so that the node could see the changes made
   by the other nodes and vice versa, because the local version of the volume
   is used by file operations performed by the node. The files and directories
   are being synchronized in two directions. The local versions get updated
   according to the master versions and the local modifications get reintegrated
   to the volume master.

   \subsection modification_log Modification Log

   The file system needs to maintain a modification log for volumes that are
   cached on a local node so that it could support disconnected operation and
   could reintegrate the local changes effectively. Moreover, if both the local
   node and the volume master made changes, it would be impossible to decide
   what changes were made by the local node and what changes were made by
   the volume master without the modification log.

   The log should contain all operations modifying the state of file system.
   The question is whether the file system should employ one log for all volumes
   cached on the local node, one for each cached volume or a separate log for
   every directory and file.

   If one log was used for all volumes or one for each volume, it would be
   expensive to search for records for a specific file or directory when reintegrat-
   ing a given file or directory. If one log was used for all volumes, it would also
   be ineficient to delete corresponding log entries when the node chooses not
   to cache the volume anymore, or the file system would simply ignore such
   entries. On the other hand, searching for the corresponding entries is trivial
   when using separate modification logs for every file or directory. Although
   it is slightly more dificult to manage the number of log files, it is a better
   solution. For example, we can easily delete useless log entries.

   Another reason, which is another argument for the usage of separate logs
   for each file and directory, is that there are only few types of log entries as
   described below.

     1. In a directory, a user can only add or delete a directory entry. It is ob-
        vious that simple operations like create or unlink can be represented
        by these log records. But it is possible to represent the more compli-
        cated operations too: link simply adds a new directory entry for the
        same file, rename deletes one directory entry and inserts another one.

      2. For a file opened for writing, the only type of log record is a modification
         of data in an interval of offsets of the file. The data do not need to be
         stored to log because they are in the file.

      3. The user may also change the attributes (ownership and access rights)
         of a file or a directory. This does not need to be written to the log, it
         is enough to remember the old attributes that the file had when it was
         last updated. The old attributes may be kept in metadata, the new
         attributes are in the file system.

   \subsection detecting_the_modifications  Detecting the Modifications

   While the file system operations are being processed, the file system checks
   whether the files or directories used by the operation should get synchronized.
   It is essential to find out whether the file has changed on the volume master
   or on the local node and how it has changed. Eventually, the update or
   reintegration is started, or the conflict is represented in the file system.

   Because of the reasons described in the previous section, it is necessary
   to maintain a modification log for the changes that are to be reintegrated to
   volume master. The question is what is the best way to detect the changes
   made by the volume master.

   The first idea is to use a modification log on the volume master to update
   the local copy of the volume. The obsoleted records in the log have to be
   deleted so that the log would not occupy the whole disk. But the log entries
   are not obsoleted while other nodes still need them. So the volume master
   would have to wait until all its descendants in the volume hierarchy update
   the corresponding files. But the descendants may be disconnected for an
   unlimited amount of time, or they may be “dead” and not deleted from the
   list of nodes. Therefore, the log could have an unlimited size and this is what
   should be avoided.

   Another option is to change the file handle1 after each modification of the
   file. This is not a good solution because the file identifier is used to identify
   a specific file. Therefore, it would look like the file was deleted and new one
   was created, which would cause more expensive synchronization.

   The best solution is to assign each file a version number and increase it
   each time the file gets modified. It is simple to find out what file has changed
   from the version numbers. The local file has changed when its version number
   is greater than the master's version number that the file had when it was last
   updated. The file on the volume master has been modified when the master's
   version number has changed since the file was last updated.

   The version numbers are needful only for regular files and directories
   because these are the only file system entities that ought to be synchronized.
   Character devices, sockets and pipes do not have internal data, symlinks
   have to be deleted and recreated to change their contents, and finally it is
   not a good idea to synchronize the contents of block devices because the
   same devices do not have to be attached to all nodes.

   \subsection update  Update

   As described earlier, the modifications of a file on node's volume master are
   detected only by comparing the version numbers of the file. So it is necessary
   to find out what exactly has changed. The method is different for regular
   files and directories.

   When updating a regular file, it is necessary to update only the parts of
   the file that have not been updated yet and were not modified by the local
   node. These parts can be updated in several ways.

   First option is to simply get all the wanted parts of the file and write the
   data to the local version of the file. But this solution fetches also the blocks
   that were not modified by the volume master so it may cause useless network
   trafic.

   A better solution is to fetch only the parts that were modified by the
   volume master. In order to do so, the hash sums of small blocks2 would be
   computed by the local node and by the volume master. If the hash sums
   differ, the corresponding blocks differ too and have to be updated. This
   solution is used by ZFS.

   When updating a directory, the changes made by the volume master have
   to be found and then performed in the local cache. To find the changes, the
   contents of the local and the master directory have to be read and compared
   while ignoring the changes made by the local node.

   Regular files are being updated in the background when the local node is
   connected to the volume master via a fast network, and they are accessible
   during the update. If a user wants to read a block that has not been updated
   yet it is fetched from the volume master. On the other hand, the update of a
   directory has to finish before the file system operation can continue because
   it must see the updated directory.
  
   When updating a directory, it is not a good idea to completely update
   all files and directories that the directory contains, because the update of
   the volume root would cause the update of the whole volume. Therefore,
   the files and directories that are contained in the directory being updated
   are created as empty, and they get updated when the user opens them or
   performs a directory operation in them.

   \subsection reintegration  Reintegration

   When a file or directory was modified by the local node, it is necessary to
   reintegrate the changes to the volume master. The reintegration is easier than
   the update because the changes are described in the modification log.
   To do so, the appropriate log entries are read, the operations
   are invoked on the volume master, the log entries are deleted, and the file
   versions are updated.

   Similarly to the update, the regular files are being reintegrated in the
   background when the local node is connected to its volume master via a fast
   network, and it is possible to access them meanwhile. The directories have to
   be reintegrated before the file system operation can continue. The reason is
   that a conflict can be detected and represented in the file system during the
   reintegration, and the operation should see the conflict.

   When the reintegration is started, it would be possible to reintegrate
   everything modified or just the file or directory being accessed. Both options
   have several advantages and disadvantages as follows.

   When all modifications are being reintegrated at once, it is good that
   the modifications are visible to the other nodes as soon as possible. On the
   other hand, the file system operation has to be delayed until at least all
   directories are reintegrated, which could take a long time especially when a
   disconnected node that made many changes connects again. It would also be
   very complicated to allow an execution of other file system operations during
   the reintegration.

   It does not take such a long time to reintegrate the directory that is used
   by the file system operation. This type of invocation of the reintegration is
   also similar to the invocation of the update. This method is much better
   than the previous one when separate logs for each file and directory are used.
   However, all modified directories and files have to be visited in order to
   reintegrate them, but the user can do this manually. To help the user, ZFS
   could provide a special tool that would reintegrate all modification logs. With
   respect to the previous reasons, this type of invocation is used by ZFS.

   \section conflicts Conflicts

   Conflicts appear when node N, which caches a volume, starts to update or
   reintegrate a file or a directory and another node has invoked conflicting
   operations since the last time node N had updated and reintegrated the file
   or directory. So the conflicts mostly appear when a disconnected node that
   made changes connects again and tries to perform an operation on the mod-
   ified file or directory.

   \subsection representation_of_conflicts Representation of Conflicts

   Coda represents a conflicting file as a symlink to its file identifier, which
   makes the contents of the file inaccessible. According to Coda manual,
   a user needs to run the utility called repair and type several commands to
   convert this symlink to a directory and to fix the conflict. This is a rather
   complicated solution.

   To avoid usage of a special utility, it would be necessary to represent the
   conflict with the conflicting versions in the file system. This would enable
   the user to see and access the versions, compare them like usual files and
   resolve conflicts by deleting the unwanted versions.

   The first idea how to represent the conflict is to create a real directory
   and place all versions of the conflicting file into it, the names of versions
   would be the names of nodes. This approach does not work for conflicts
   on directories. When the directory is in an attribute - attribute conflict, 
   the directory representing the conflict would contain the
   versions of the conflicting directory. All versions of the conflicting directories
   should have the same contents so the contents should be hard-linked between
   the directories. However, the conflicting directory may contain a subdirectory
   but directories cannot be linked in Linux and many other operating systems.

   Similar problems arise when the conflict would be represented by several
   files whose names would be a concatenation of the original name and the
   name of the corresponding node. Another problem of this solution would
   be that such names might already exist in the directory that contains the
   conflicting file.

   Still, representing the conflict by a directory that contains the conflicting
   versions is a good idea because it is obvious which files are in the conflict.
   The directory has to be a virtual directory because of the reasons described
   above. It exists only in memory structures and contains directory entries
   that are used to directly access the appropriate versions of the file.

   Another question is whether the user should see all conflicting versions of
   the file, or just the version on the local node and the version on the volume
   master of the local node.

   At the first sight, it would be better to show all conflicting versions. But
   it would be very ineficient because the versions are not in one place as they
   would be if we used a real directory for the conflict. Node would need to
   check the version numbers of the file on all reachable nodes each time it
   tries to access the file, or the node would do that only for the first time and
   then other nodes would tell the versions of their files when they connect or
   modify the file. Another possibility would be to dedicate one of the nodes
   to be a conflict manager for the volume, the other nodes would report their
   version numbers to this manager when they connect or modify a file. All
   these solutions result in high network trafic.

   Therefore, it is better to show only the local version and the version on
   the volume master because the node has to compare the versions only on
   these two nodes. Thus the conflict is created only on a node that caused
   it, which has another advantage of not annoying the other nodes with the
   conflict that the node created and should resolve.

   According to final decisions, the conflicts are represented in the file system
   as a virtual directory whose name is the same as the name of the conflicting
   file and the virtual directory is located in the file's place. In this virtual
   directory, the local and master version of the file are located and each of
   them is named according to name of the node on which the version is. If
   the conflict is caused by deleting one version and modifying the other, the
   deleted version is represented by a virtual symlink to the existing version.
   For example, when a regular file foo on node1 is in conflict with a character
   device foo on node2, there is a virtual directory foo in place of file foo and
   it contains the regular file node1 and the character device node2.

   \subsection types_of_conflicts Types of Conflicts

   There are several types of conflicts as described below.

   attribute - attribute conflicts appear when the local node has changed
        the file attributes (access rights, user ID of the owner and group ID of
        the owner) of a generic file4 in a different way than the node's volume
        master. A file may be in an attribute - attribute conflict and another
        type of conflict at the same time.

   modify - modify conflicts (version conflicts) arise when the local node
       has modified the contents of a regular file and the volume master has
       modified it too.

   create - create conflicts (file handle conflicts) turn up when there is a
        directory entry with the same name but different file handles on the
        local node and on the volume master. They are a result of a conflicting
        create, mkdir, mknod, symlink, link or rename, or of a situation when
        at least one node deleted a generic file and created another one with
        the same name5 .

   modify - delete conflicts come out when the local node has modified a
       regular file while the volume master has deleted it.

   delete - modify conflicts occur when the local node has deleted a regular
        file while the volume master has modified it.


   \section configuration_manager Configuration Management

   In order to do its job, the file system has to know certain information, for
   example the list of nodes and volumes, what node it should contact to access a
   given volume etc. This configuration of the file system needs to be distributed
   to all nodes. There are two main options how to do this.

   The first option is to employ a separate configuration manager, which
   is requested by other nodes to send them the configuration or to change
   it. Coda uses this option . But why should there be another system
   providing access to some information when there already is one?

   Therefore, the better choice is to use the file system itself to manage its
   own configuration. The configuration must be located in a fixed place so that
   it could be found. The possibilities are a predefined path from the root of
   file system to the directory containing the configuration, a predefined path
   from the root of a specific volume, or a separate volume. The best solution
   is to store all configuration files in a separate configuration volume.

   Another question is whether the configuration volume should be accessed
   remotely or cached. If it was accessed remotely, each node would need to
   know from which node it should read the configuration. It would have to
   know the host name of the node. The ID or name of the node would not be
   enough because the node needs the configuration to convert the ID or name
   to the host name. The host name would also have to be stored out of the
   file system, therefore it would be dificult to update it when the hierarchy of
   the configuration volume changes.

   On the other hand, when the volume would be cached on the local disk,
   the file system could read the complete configuration without any interaction
   with other nodes and build the in-memory data structures. However, the
   cached configuration may be outdated. So the file system has to start reading
   the configuration once again, which causes the configuration to be updated
   according to the node's volume master and to refresh the in-memory data
   structures.

   Although the disconnected node knows where in the local file system the
   contents of volumes cached or provided by the node are located, it needs
   to read the complete configuration to be able to work in a usual way, for
   example it needs to know where the volumes are mounted.

   Because of the previous reasons, the final decision is that the configuration
   is stored in the configuration volume, which is cached by all nodes.

   \subsection updating_the_configuration Updating the Configuration

   When a user of the file system changes a part of the configuration, the file
   system should ensure that all nodes refresh their configuration. As described
   in the previous section, the node that is starting the file system updates its
   configuration during the startup. So it is needed to describe how do the
   running nodes update their configuration.

   The obvious option is to periodically check the versions of all configuration
   files and reread the ones whose versions have changed. However, this is not a
   good solution because all nodes would send many useless messages and would
   notice the change of configuration after non-trivial delay. The configuration
   also does not change too often.

   A better solution than polling is to ensure that all nodes reread the
   changed parts of configuration as soon as possible after the modifications
   happen. First, the changes have to be detected by the node that made them.
   This can be done by adding hooks to functions accessing the local files of the
   volume. Then, the other nodes have to be notified. It is enough to notify the
   nodes that are the direct descendants in the volume hierarchy.
   When they receive the notification, they update the corresponding
   files and thus modify them and send the notification to their descendants.

   The whole subtree of the node that changed the configuration refreshes its
   configuration this way. The rest of the hierarchy is notified as follows. When
   the node changes the configuration, it reintegrates the files to its volume
   master (the ancestor in the hierarchy). Thus the volume master modifies its
   files, notifies its descendants except the one that has reintegrated the file,
   and reintegrates the file to its ancestor. The result is that all nodes were
   notified, and have updated and reread their configuration.

   \subsection adding_a_new_node Adding a New Node

   It is possible to do most of the changes of configuration, for example to add a
   volume or choose to cache a volume, by editing the files on the configuration
   volume. But it is more complicated to add a new node because the new node
   does not have the configuration yet.

   New nodes can be added only by the current ones so that there would
   be a control what nodes are able to access the file system. An existing
   node adds information about the new node to the configuration. Because
   the new nodes do not have the configuration of the file system cached on
   their disks, they require a way to fetch the configuration9 . So the file system
   provides a possibility to specify from which node the new node should get
   the configuration for the first time10 . During the startup, the file system will
   fetch the configuration from the given node and cache it on the local disk.

*/
