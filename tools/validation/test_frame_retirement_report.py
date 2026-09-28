import csv
from pathlib import Path
import tempfile
import unittest
from frame_retirement_report import FIELDS, UNKNOWN, read_trace


class Retirement(unittest.TestCase):
    def rows(self):
        base=dict(schema_version=1,sequence=0,record='interval',renderer_frame=42,scope='world',stage='frame_fence',
                  thread_id=7,start_ns=100,end_ns=200,wall_ms=.0001,thread_cpu_ns=0,slot=-1,fence_value=0,
                  source_frame=UNKNOWN,completed_before=UNKNOWN,completed_after=UNKNOWN,wait_called=0,success=1,
                  result=0,interval_count=0)
        rows=[base]
        for index,name in enumerate(('fence_precheck','fence_wait_call','fence_postcheck')):
            rows.append(base | dict(sequence=index+1,record='fence',stage=name,start_ns=110+index*20,
                end_ns=130+index*20,wall_ms=.00002,slot=0,fence_value=9,source_frame=40,
                completed_before=8,completed_after=9,wait_called=1,thread_cpu_ns=-1))
        rows.append(base | dict(sequence=4,record='frame',stage='complete',start_ns=90,end_ns=210,
                               wall_ms=.00012,thread_cpu_ns=-1,interval_count=1))
        return rows

    def validate(self,rows,gpu_frames=(42,),log='frame_cpu_profile enabled=1 schema=1 bounded_intervals=128'):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'trace.csv';gpu=Path(directory)/'gpu.csv'
            with path.open('w',newline='') as stream:
                writer=csv.DictWriter(stream,FIELDS);writer.writeheader();writer.writerows(rows)
            gpu.write_text('frame\n'+''.join(str(f)+'\n' for f in gpu_frames))
            return read_trace(path,log,gpu)

    def test_nested_wait_not_double_counted_and_source_exact(self):
        report=self.validate(self.rows());frame=report['worst_world_frames'][0]
        self.assertAlmostEqual(frame['recorded_primary_ms'],.0001)
        self.assertEqual(frame['fence_waits'][0]['source_frame'],40)
        self.assertEqual(frame['fence_waits'][0]['fence_value'],9)

    def test_missing_incomplete_and_wrong_join_fail(self):
        rows=self.rows()
        for invalid in (rows[:-1],rows[1:],rows[:2]+rows[3:],rows[:-1]+[rows[-1]|dict(interval_count=2)],
                        rows[:-1]+[rows[-1]|dict(stage='unwound')]):
            with self.subTest(invalid=invalid),self.assertRaises(ValueError):self.validate(invalid)
        with self.assertRaises(ValueError):self.validate(rows,gpu_frames=(43,))

    def test_error_activation_and_nonfinite_rejected(self):
        for log in ('','frame_cpu_profile enabled=1 schema=1 profile_writer_failed'):
            with self.assertRaises(ValueError):self.validate(self.rows(),log=log)
        rows=self.rows();rows[0]['wall_ms']=float('nan')
        with self.assertRaises(ValueError):self.validate(rows)

    def test_unsafe_or_inconsistent_fence_completion_rejected(self):
        for change in (dict(completed_after=8),dict(completed_before=UNKNOWN),dict(wait_called=0),dict(success=0),dict(result=-1)):
            rows=self.rows()
            for r in rows[1:4]:r.update(change)
            with self.subTest(change=change),self.assertRaises(ValueError):self.validate(rows)


if __name__=='__main__':unittest.main()
