# coding: utf-8

# Copyright (C) 1994-2018 Altair Engineering, Inc.
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

from tests.functional import *


class TestMomDynRes(TestFunctional):

    filenames = []

    def create_mom_resources(self, resc_name, resc_type, resc_flag,
                             script_body):
        """
        helper function for creating mom_resource.
        """
        fp_list = []
        resc_name_mom = ",".join(resc_name)
        resc_name_svr = resc_name_mom
        resc_name_mom = '"' + resc_name_mom + '"'
        self.scheduler.set_sched_config({'mom_resources': resc_name_mom},
                                        validate=True)
        self.scheduler.add_resource(resc_name_svr)

        ln = len(resc_name)
        for i in range(ln):
            attr = {"type": resc_type[i], "flag": resc_flag[i]}
            self.server.manager(MGR_CMD_CREATE, RSC, attr,
                                id=resc_name[i], expect=True)

            fp = self.du.create_temp_file(prefix="mom_resc", suffix=".scr",
                                          body=script_body[i])
            self.du.chmod(path=fp, mode=0755)
            mom_config_str = "!" + fp
            self.mom.add_config({resc_name[i]: mom_config_str})
            fp_list.append(fp)
            self.filenames.append(fp)
        return fp_list

    def test_res_string_incorrect_value(self):
        """
        Test for host level string resource
        with string mom dynamic value
        """

        resc_name = ["foo"]
        resc_type = ["string"]
        resc_flag = ["h"]
        script_body = ["/bin/echo pqr"]

        self.create_mom_resources(resc_name, resc_type, resc_flag, script_body)

        # Submit a job that requests different mom dynamic resource
        # not return from script
        attr = {"Resource_List." + resc_name[0]: 'abc'}
        j = Job(TEST_USER, attrs=attr)
        jid = self.server.submit(j)

        # The job shouldn't run
        c = "Can Never Run: Insufficient amount of resource: foo (abc != pqr)"
        self.server.expect(JOB, {'job_state': 'Q', 'comment': c},
                           id=jid, attrop=PTL_AND)

    def test_res_string_array_value(self):
        """
        Test for host level string_array resource
        with string mom dynamic value
        """

        resc_name = ["foo"]
        resc_type = ["string_array"]
        resc_flag = ["h"]
        script_body = ["/bin/echo red,green,blue"]

        self.create_mom_resources(resc_name, resc_type, resc_flag, script_body)

        # Submit a job that requests mom dynamic resource
        attr = {"Resource_List." + resc_name[0]: 'red'}
        j = Job(TEST_USER, attrs=attr)
        jid = self.server.submit(j)

        # The job should run
        self.server.expect(JOB, {'job_state': 'R'}, id=jid)

        # Submit a job that requests different mom dynamic resource
        # not return from script
        attr = {"Resource_List." + resc_name[0]: 'white'}
        j = Job(TEST_USER, attrs=attr)
        jid = self.server.submit(j)

        # The job shouldn't run
        c = "Can Never Run: Insufficient amount of resource:"
        c += " foo (white != red,green,blue)"
        self.server.expect(JOB, {'job_state': 'Q', 'comment': c},
                           id=jid, attrop=PTL_AND)

    def test_res_string_perticular_value(self):
        """
        Test for host level string resource
        with mom dynamic value “This is a test”
        """

        resc_name = ["foo"]
        resc_type = ["string"]
        resc_flag = ["h"]
        script_body = ["/bin/echo This is a test"]

        self.create_mom_resources(resc_name, resc_type, resc_flag, script_body)

        # Submit a job that requests different mom dynamic resource
        attr = {"Resource_List." + resc_name[0]: 'red'}
        j = Job(TEST_USER, attrs=attr)
        jid = self.server.submit(j)

        # The job shouldn't run
        c = "Can Never Run: Insufficient amount of resource:"
        c += " foo (red != This is a test)"
        self.server.expect(JOB, {'job_state': 'Q', 'comment': c},
                           id=jid, attrop=PTL_AND)

        # Submit a job that requests mom dynamic resource
        attr = {"Resource_List." + resc_name[0]: '"This is a test"'}
        j = Job(TEST_USER, attrs=attr)
        jid = self.server.submit(j)

        # The job should run
        self.server.expect(JOB, {'job_state': 'R'}, id=jid)

    def test_res_string_correct_value(self):
        """
        Test for host level string resource
        with string mom dynamic value
        """

        resc_name = ["foo"]
        resc_type = ["string"]
        resc_flag = ["h"]
        script_body = ["/bin/echo abc"]

        self.create_mom_resources(resc_name, resc_type, resc_flag, script_body)

        # Submit a job that requests mom dynamic resource
        attr = {"Resource_List." + resc_name[0]: 'abc'}
        j = Job(TEST_USER, attrs=attr)
        jid = self.server.submit(j)

        # The job should run
        self.server.expect(JOB, {'job_state': 'R'}, id=jid)

    def test_res_float_value(self):
        """
        host level consumable float resource with mom dynamic value float
        """

        resc_name = ["foo"]
        resc_type = ["float"]
        resc_flag = ["nh"]
        script_body = ["/bin/echo 3"]

        self.create_mom_resources(resc_name, resc_type, resc_flag, script_body)

        # Submit a job that requests more value for the mom resource
        # than available
        attr = {"Resource_List." + resc_name[0]: 4}
        j = Job(TEST_USER, attrs=attr)
        jid = self.server.submit(j)

        # The job shouldn't run
        c = "Can Never Run:"
        c += " Insufficient amount of resource: foo (R: 4 A: 3 T: 3)"
        self.server.expect(JOB, {'job_state': 'Q', 'comment': c},
                           id=jid, attrop=PTL_AND)

        # Submit a job that requests less value for the mom resource
        # than available
        attr = {"Resource_List." + resc_name[0]: 2}
        j = Job(TEST_USER, attrs=attr)
        jid = self.server.submit(j)

        # The job should run
        self.server.expect(JOB, {'job_state': 'R',
                           'Resource_List.'+resc_name[0]: '2'},
                           id=jid, attrop=PTL_AND)

    def test_multiple_res_valid_value(self):
        """
        multiple host level resource long,float with all correct values.
        Submit different jobs with different resource types
        """

        resc_name = ["foo", "foo1"]
        resc_type = ["float", "long"]
        script_body = ["/bin/echo 3", "/bin/echo 4"]
        resc_flag = ["nh", "nh"]

        self.create_mom_resources(resc_name, resc_type,
                                  resc_flag, script_body)

        # Submit a job that requests mom dynamic resource
        attr = {"Resource_List." + resc_name[0]: 2,
                "Resource_List." + resc_name[1]: 3}
        j = Job(TEST_USER, attrs=attr)
        jid = self.server.submit(j)

        # The job should run
        self.server.expect(JOB, {'job_state': 'R'}, id=jid)

        # Submit a job that requests more value
        # than available for both mom resources.
        attr = {"Resource_List." + resc_name[0]: 4,
                "Resource_List." + resc_name[1]: 4}
        j = Job(TEST_USER, attrs=attr)
        jid = self.server.submit(j)

        # The job shouldn't run
        c = "Can Never Run:"
        c += " Insufficient amount of resource: foo (R: 4 A: 3 T: 3)"
        self.server.expect(JOB, {'job_state': 'Q', 'comment': c},
                           id=jid, attrop=PTL_AND)

    def test_multiple_res_invalid_value(self):
        """
        multiple host level resource with one or more incorrect values.
        Submit different jobs with different resource types
        """

        resc_name = ["foo", "foo1"]
        resc_type = ["float", "size"]
        script_body = ["/bin/echo 3", "/bin/echo -2"]
        resc_flag = ["nh", "nh"]

        self.create_mom_resources(resc_name, resc_type,
                                  resc_flag, script_body)

        # Submit a job that requests mom dynamic resource
        attr = {"Resource_List." + resc_name[0]: 2,
                "Resource_List." + resc_name[1]: 1}
        j = Job(TEST_USER, attrs=attr)
        jid = self.server.submit(j)

        # The job shouldn't run
        c = "Can Never Run:"
        c += " Insufficient amount of resource: foo1 (R: 1kb A: 0kb T: 0kb)"
        self.server.expect(JOB, {'job_state': 'Q', 'comment': c},
                           id=jid, attrop=PTL_AND)

    def test_delete_res(self):
        """
        delete a resource while jobs are running/queued.
        """

        message = "Resource busy on job"
        resc_name = ["foo"]
        resc_type = ["string"]
        resc_flag = ["h"]
        script_body = ["/bin/echo abc"]

        self.create_mom_resources(resc_name, resc_type, resc_flag, script_body)

        # Submit a job that requests mom dynamic resource
        attr = {"Resource_List." + resc_name[0]: 'abc'}
        j = Job(TEST_USER, attrs=attr)
        jid = self.server.submit(j)

        # The job should run
        self.server.expect(JOB, {'job_state': 'R'}, id=jid)

        # Check for the expected error message throwing by PBS
        try:
            self.server.manager(MGR_CMD_DELETE, RSC, id='foo')
        except PbsManagerError as e:
            self.assertTrue(message in e.msg[0])

    def test_alter_res_value(self):
        """
        Verify that altered mom dynamic resource value will take affect
        only when a job is rerun
        """

        resc_name = ["foo"]
        resc_type = ["float"]
        resc_flag = ["nh"]
        script_body = ["/bin/echo 3"]

        filename = self.create_mom_resources(resc_name, resc_type,
                                             resc_flag, script_body)

        # Submit a job that requests mom dynamic resource
        attr = {"Resource_List." + resc_name[0]: '2'}
        j = Job(TEST_USER, attrs=attr)
        jid = self.server.submit(j)

        # The job should run
        self.server.expect(JOB, {'job_state': 'R'}, id=jid)

        with open(filename[0], "wb") as fd:
            fd.truncate()
            fd.write("echo 1")

        self.server.rerunjob(jobid=jid)

        # The job shouldn't run
        c = "Can Never Run:"
        c += " Insufficient amount of resource: foo (R: 2 A: 1 T: 1)"
        self.server.expect(JOB, {'job_state': 'Q', 'comment': c},
                           id=jid, attrop=PTL_AND)

    def tearDown(self):
        #removing all files creating in test
        for i in self.filenames:
            if os.path.isfile(i):
                os.remove(i)
