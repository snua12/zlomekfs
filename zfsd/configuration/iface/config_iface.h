/**
 *  @file
 * 
 *  @section DESCRIPTION
 *  Interface between configuratinon and others parts of ZlomkeFS
 *
 */

#ifndef CONFIG_IFACE_H
#define CONFIG_IFACE_H

/* ! \brief Structure for storing informations about volumes from shared config. */
typedef struct volume_entry_def
{
	uint32_t id; /* !< ID of volume */
	string name; /* !< name of volume */
	string mountpoint; /* !< mountpoint of volume */
	string master_name; /* !< name of master node */
	varray slave_names; /* ! names of slave nodes */
}
volume_entry;

typedef struct user_mapping_def
{
	string zfs_user;
	string node_user;
}
user_mapping;

typedef struct group_mapping_def
{
	string zfs_group;
	string node_group;
}
group_mapping;

#endif
