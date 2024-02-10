env-install:
   micromamba env create --file environment.yml

run:
   micromamba run -n sciqlop python -m SciQLop.app