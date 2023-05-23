import pandas
import numpy

data = pandas.read_csv("./data/1_bill", header=None)
hist = numpy.histogram(data, 80591, range=(0, 10000000))
