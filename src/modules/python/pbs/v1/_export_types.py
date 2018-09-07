# coding: utf-8
"""

# Copyright (C) 1994-2019 Altair Engineering, Inc.
# For more information, contact Altair at www.altair.com.
#
# This file is part of the PBS Professional ("PBS Pro") software.
#
# Open Source License Information:
#
# PBS Pro is free software. You can redistribute it and/or modify it under the
# terms of the GNU Affero General Public License as published by the Free
# Software Foundation, either version 3 of the License, or (at your option) any
# later version.
#
# PBS Pro is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.
# See the GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# Commercial License Information:
#
# For a copy of the commercial license terms and conditions,
# go to: (http://www.pbspro.com/UserArea/agreement.html)
# or contact the Altair Legal Department.
#
# Altair’s dual-license business model allows companies, individuals, and
# organizations to create proprietary derivative works of PBS Pro and
# distribute them - whether embedded or bundled with other software -
# under a commercial license agreement.
#
# Use of Altair’s trademarks, including but not limited to "PBS™",
# "PBS Professional®", and "PBS Pro™" and Altair’s logos is subject to Altair's
# trademark licensing policies.

"""

__doc__ = r"""
This module primarily exists to help the embedded interpreter import all the
python types. All attribute names that require *special* handling are maintained
in a dictionary as:
    Key <name> : Value <python_type>
    
    where:
        name *MUST* map to either attribute_def.at_name or resource_def.rs_name

Motivation:
    Since most of the object will be generated by the embedded python (aka C code)
    this gives a simple way to leverage the Python Data types to pass the infor-
    mation back to the embedded python. Which, then can use the excellent C API
    to retrieve the python type.
    
"""

from . import _base_types as pbs_types
from ._svr_types import (_queue, _job, _server, _resv, _vnode, _event, pbs_iter)
from ._exc_types import *


#: IMPORTANT the keys are imported by the C code, make sure the mapping is
#: maintained.
#: TODO In future or time permits should add a constants to the internal _pbs_v1
#: module.
#:
EXPORTED_TYPES_DICT = {
                       'interactive' 		: pbs_types.pbs_int,
                       'block'			: pbs_types.pbs_int,
                       'Authorized_Users' 	: pbs_types.acl,
                       'Authorized_Groups' 	: pbs_types.acl,
                       'Authorized_Hosts' 	: pbs_types.acl,
                       'array_index' 		: pbs_types.pbs_int,
                       'array_indices_submitted': pbs_types.range,
                       'array_state_count'	: pbs_types.state_count,
                       'group_list' 		: pbs_types.group_list,
                       'managers' 		: pbs_types.acl,
                       'operators' 		: pbs_types.acl,
                       'User_List' 		: pbs_types.user_list,
                       'Shell_Path_List' 	: pbs_types.path_list,
                       'Output_Path' 		: pbs_types.path,
                       'Error_Path' 		: pbs_types.path,
                       'Priority' 		: pbs_types.priority,
                       'Job_Name' 		: pbs_types.name,
                       'project' 		: pbs_types.project,
                       'Reserve_Name' 		: pbs_types.name,
                       'Join_Path' 		: pbs_types.join_path,
                       'default_qsub_arguments' : pbs_types.args,
                       'default_qdel_arguments' : pbs_types.args,
                       'select'             : pbs_types.select,
                       'schedselect'        : pbs_types.select,
                       'place'              : pbs_types.place,
                       'exec_host'          : pbs_types.exec_host,
                       'exec_vnode'         : pbs_types.exec_vnode,
                       'Checkpoint'         : pbs_types.checkpoint,
                       'depend'             : pbs_types.depend,
                       'Hold_Types'         : pbs_types.hold_types,
                       'Keep_Files'         : pbs_types.keep_files,
                       'Mail_Points'        : pbs_types.mail_points,
                       'Mail_Users'         : pbs_types.email_list,
                       'stagein'       	    : pbs_types.staging_list,
                       'stageout'           : pbs_types.staging_list,
                       'range'              : pbs_types.range,
                       'state_count'        : pbs_types.state_count,
                       'license_count'      : pbs_types.license_count,
                       'scheduler_iteration': pbs_types.duration,
                       'reserve_duration'   : pbs_types.duration,
                       'args'               : pbs_types.args,
                       'job_sort_formula'   : pbs_types.job_sort_formula,
                       'node_group_key'     : pbs_types.node_group_key,
                       'sandbox'	    : pbs_types.sandbox,
                       'pbs_version'        : pbs_types.version,
                       'software'           : pbs_types.software,
                       'acl_roots'          : pbs_types.acl,
                       'acl_hosts'          : pbs_types.acl,
                       'acl_resv_hosts'     : pbs_types.acl,
                       'acl_resv_groups'    : pbs_types.acl,
                       'acl_resv_users'     : pbs_types.acl,
                       'acl_groups'         : pbs_types.acl,
                       'acl_users'          : pbs_types.acl,
                       'state_count'        : pbs_types.state_count,
                       'server_state'       : pbs_types.server_state,
                       'route_destinations' : pbs_types.route_destinations,
		       'Variable_List'	    : pbs_types.pbs_env,	
		       'queue_type'	    : pbs_types.queue_type,	
		       'job_state'	    : pbs_types.job_state,	
                       'license'            : pbs_types.pbs_str,
                       'license_info'       : pbs_types.pbs_int,
                       'attr_descriptor'    : pbs_types.PbsAttributeDescriptor,
                       'generic_type'       : pbs_types._generic_attr,
                       'pbs_bool'           : pbs_types.pbs_bool,
                       'pbs_int'            : pbs_types.pbs_int,
                       'pbs_env'            : pbs_types.pbs_env,
                       'pbs_list'           : pbs_types.pbs_str,
                       'pbs_str'            : pbs_types.pbs_str,
                       'pbs_float'          : pbs_types.pbs_float,
                       'pbs_resource'       : pbs_types.pbs_resource,
                       'size'               : pbs_types.size,
                       'generic_time'       : pbs_types.duration,
                       'generic_acl'        : pbs_types.acl,
                       'queue'              : _queue,
                       'default_queue'      : _queue,
                       'job'                : _job,
                       'server'             : _server,
                       'resv'               : _resv,
                       'vnode'              : _vnode,
                       'event'              : _event,
		       'pbs_iter'	    : pbs_iter,
		       'state'   	    : pbs_types.vnode_state,
		       'sharing'   	    : pbs_types.vnode_sharing,
		       'ntype'   	    : pbs_types.vnode_ntype,
		       'pbs_entity'   	    : str,
                'EventIncompatibleError'    : EventIncompatibleError,
                'UnsetAttributeNameError'   : UnsetAttributeNameError,
                'BadAttributeValueTypeError': BadAttributeValueTypeError,
                'BadAttributeValueError'    : BadAttributeValueError,
                'UnsetResourceNameError'    : UnsetResourceNameError,
                'BadResourceValueTypeError' : BadResourceValueTypeError,
                'BadResourceValueError'     : BadResourceValueError
                       
                      }


