from . import kfext_sim as ext
from extensions import EXTENSION_REGISTRY_MD, EXTENSION_REGISTRY_TD
EXTENSION_REGISTRY_MD.register_extension('sim', ext.MD)
EXTENSION_REGISTRY_TD.register_extension('sim', ext.TD)
