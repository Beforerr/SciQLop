import traceback
from SciQLopBindings import PyDataProvider, Product, VectorTimeSerie, ScalarTimeSerie, DataSeriesType
import numpy as np
from speasy.cache import _cache
from speasy.common.datetime_range import DateTimeRange
from datetime import datetime, timedelta, timezone
from speasy.common.variable import SpeasyVariable
from speasy.amda import AMDA

amda = AMDA()


def vp_make_scalar(var=None):
    if var is None:
        return (((np.array([]), np.array([])), np.array([])), DataSeriesType.SCALAR)
    else:
        return (((var.time, np.array([])), var.data), DataSeriesType.SCALAR)

class DemoVP(PyDataProvider):
    def __init__(self):
        super().__init__()
        self.register_products([Product("/VP/thb_fgm_gse_mod",[],{"type":"scalar"})])

    def get_data(self,metadata,start,stop):
        try:
            tstart = datetime.fromtimestamp(start, tz=timezone.utc)
            tend = datetime.fromtimestamp(stop, tz=timezone.utc)
            thb_bs = amda.get_parameter(start_time=tstart, stop_time=tend, parameter_id='thb_bs', method="REST")
            thb_bs.data = np.sqrt((thb_bs.data*thb_bs.data).sum(axis=1))
            return vp_make_scalar(thb_bs)
        except Exception as e:
            print(traceback.format_exc())
            print(f"Error in {__file__} ",str(e))
            return (((np.array([]), np.array([])), np.array([])), ts_type)


t=DemoVP()

