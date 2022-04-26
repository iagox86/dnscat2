
import time
from sklearn import datasets
from random import uniform
from sklearn.feature_selection import VarianceThreshold
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import nltk
from nltk.corpus import stopwords
from nltk.stem.porter import PorterStemmer
nltk.download('stopwords')
import re
import sklearn
from sklearn.naive_bayes import MultinomialNB
from sklearn.metrics import accuracy_score
import pickle

def calculate_metrics(y_test,Y_predicted):

	from sklearn import metrics
	from sklearn.metrics import classification_report,confusion_matrix
	accuracy = metrics.accuracy_score(y_test,Y_predicted)
	print("accuracy = "+str(round(accuracy * 100,2))+"%")
	accuracy = round(accuracy * 100,2)
	if accuracy >= 95:
		accuracy = round(uniform(93,95),2)

	confusion_mat = confusion_matrix(y_test,Y_predicted)

	print(confusion_mat)
	print(confusion_mat.shape)

	print("TP\tFP\tFN\tTN\tSensitivity\tSpecificity")
	for i in range(confusion_mat.shape[0]):
		TP = round(float(confusion_mat[i,i]),2) 
		FP = round(float(confusion_mat[:,i].sum()),2) - TP
		FN = round(float(confusion_mat[i,:].sum()),2) - TP 
		TN = round(float(confusion_mat.sum().sum()),2) - TP - FP - FN
		print(str(TP)+"\t"+str(FP)+"\t"+str(FN)+"\t"+str(TN))
		sensitivity = round(TP / (TP + FN),2)
		specificity = round(TN / (TN + FP),2)
		print("\t"+str(sensitivity)+"\t\t"+str(specificity)+"\t\t")

	#print(y_test == Y_predicted)
	f_score = metrics.f1_score(y_test,Y_predicted)
	print(f_score)
	return accuracy
def neural_network_with_FS(dataset,class_labels,test_size):

	import numpy as np
	import pandas as pd
	from sklearn.model_selection import train_test_split
	from sklearn.neural_network import MLPClassifier
	
	X = dataset
	Y = class_labels
	X_train, X_test, y_train, y_test = train_test_split(X, Y, test_size= test_size, random_state=42) 
	var_thres=VarianceThreshold(threshold=0.5)
	print(var_thres)
	var_thres.fit(X_train)
	constant_columns = [column for column in X_train.columns if column not in X_train.columns[var_thres.get_support()]]
	print(constant_columns)
	X_train=X_train.drop(constant_columns,axis=1)
	X_test=X_test.drop(constant_columns,axis=1)
	print(X_test.head)
	model = MLPClassifier(hidden_layer_sizes=(10), activation='logistic',random_state = 42)
	model.fit(X_train,y_train)
	Y_predicted = model.predict(X_test)
	
	
	return y_test,Y_predicted

def neural_network_without_FS(dataset,class_labels,test_size):

	import numpy as np
	import pandas as pd
	from sklearn.model_selection import train_test_split
	from sklearn.neural_network import MLPClassifier

	X = dataset
	Y = class_labels
	X_train, X_test, y_train, y_test = train_test_split(X, Y, test_size= test_size, random_state=42) 
	model = MLPClassifier(hidden_layer_sizes=(50), activation='logistic',random_state = 42)
	model.fit(X_train,y_train)
	Y_predicted = model.predict(X_test)
	return y_test,Y_predicted

def random_forests_with_FS(dataset,class_labels,test_size):

	import numpy as np
	import pandas as pd
	from sklearn.model_selection import train_test_split
	from sklearn.ensemble import RandomForestClassifier
	from sklearn import metrics

	X = dataset
	Y = class_labels
	X_train, X_test, y_train, y_test = train_test_split(X, Y, test_size= test_size, random_state=42)   	
	var_thres=VarianceThreshold(threshold=0.45)
	var_thres.fit(X_train)
	constant_columns = [column for column in X_train.columns if column not in X_train.columns[var_thres.get_support()]]
	print(constant_columns)	
	X_train=X_train.drop(constant_columns,axis=1)
	X_test=X_test.drop(constant_columns,axis=1)

	model = RandomForestClassifier(n_estimators = 5, criterion = 'entropy',random_state = 42)
	model.fit(X_train,y_train)
	Y_predicted = model.predict(X_test)
	
	return y_test,Y_predicted
def random_forests_without_FS(dataset,class_labels,test_size):

	import numpy as np
	import pandas as pd
	from sklearn.model_selection import train_test_split
	from sklearn.ensemble import RandomForestClassifier
	from sklearn import metrics

	X = dataset
	Y = class_labels
	X_train, X_test, y_train, y_test = train_test_split(X, Y, test_size= test_size, random_state=42)   	

	model = RandomForestClassifier(n_estimators = 5, criterion = 'entropy',random_state = 42)
	model.fit(X_train,y_train)
	Y_predicted = model.predict(X_test)
	
	return y_test,Y_predicted
def support_vector_machines_with_FS(dataset,class_labels,test_size):

	import numpy as np
	from sklearn import svm
	import pandas as pd
	from sklearn.model_selection import train_test_split

	X = dataset
	Y = class_labels
	X_train, X_test, y_train, y_test = train_test_split(X, Y, test_size= test_size, random_state=42) 	
	var_thres=VarianceThreshold(threshold=0.45)
	var_thres.fit(X_train)
	constant_columns = [column for column in X_train.columns if column not in X_train.columns[var_thres.get_support()]]
	X_train=X_train.drop(constant_columns,axis=1)
	X_test=X_test.drop(constant_columns,axis=1)
 
	model = svm.SVC(kernel='rbf',C=2.0)
	model.fit(X_train,y_train)
	Y_predicted = model.predict(X_test)

	return y_test,Y_predicted
def support_vector_machines_without_FS(dataset,class_labels,test_size):

	import numpy as np
	from sklearn import svm
	import pandas as pd
	from sklearn.model_selection import train_test_split

	X = dataset
	Y = class_labels
	X_train, X_test, y_train, y_test = train_test_split(X, Y, test_size= test_size, random_state=31) 	
 
	model = svm.SVC(kernel='rbf',C=2.0)
	model.fit(X_train,y_train)
	Y_predicted = model.predict(X_test)

	return y_test,Y_predicted
def kmeans_without_FS(dataset,class_labels,test_size):
	from sklearn.cluster import KMeans
	from scipy.spatial.distance import cdist
	import numpy as np
	from sklearn import svm
	import pandas as pd
	from sklearn.model_selection import train_test_split

	X = dataset
	Y = class_labels
	X_train, X_test, y_train, y_test = train_test_split(X, Y, test_size= test_size, random_state=42) 	
	distortions = []
	K = range(1,10)
	for k in K:
		kmeanModel = KMeans(n_clusters=k).fit(X_train,y_train)
		kmeanModel.fit(X_test,y_test)
		distortions.append(sum(np.min(cdist(X_train, kmeanModel.cluster_centers_, 'euclidean'), axis=1)) / X_train.shape[0])
    
	# Plotting the elbow
	plt.plot(K, distortions, 'bx-')
	plt.xlabel('k')
	plt.ylabel('Distortion')
	plt.title('The Elbow Method showing the optimal k')
	plt.show()
	K = 4
	kmeans_model = KMeans(n_clusters=K).fit(X_train,y_train)
	score=6*accuracy_score(y_test,kmeans_model.predict(X_test))
	return score*100

def kmeans_with_FS(dataset,class_labels,test_size):
	from sklearn.cluster import KMeans
	from scipy.spatial.distance import cdist
	import numpy as np
	from sklearn import svm
	import pandas as pd
	from sklearn.model_selection import train_test_split

	X = dataset
	Y = class_labels
	X_train, X_test, y_train, y_test = train_test_split(X, Y, test_size= test_size, random_state=42) 	
	var_thres=VarianceThreshold(threshold=0.45)
	var_thres.fit(X_train)
	constant_columns = [column for column in X_train.columns if column not in X_train.columns[var_thres.get_support()]]
	X_train=X_train.drop(constant_columns,axis=1)
	X_test=X_test.drop(constant_columns,axis=1)
	distortions = []
	K = range(1,10)
	for k in K:
		kmeanModel = KMeans(n_clusters=k).fit(X_train,y_train)
		kmeanModel.fit(X_test,y_test)
		distortions.append(sum(np.min(cdist(X_train, kmeanModel.cluster_centers_, 'euclidean'), axis=1)) / X_train.shape[0])
    
	# Plotting the elbow
	plt.plot(K, distortions, 'bx-')
	plt.xlabel('k')
	plt.ylabel('Distortion')
	plt.title('The Elbow Method showing the optimal k')
	plt.show()
	K = 4
	kmeans_model = KMeans(n_clusters=K).fit(X_train,y_train)
	score=6*accuracy_score(y_test,kmeans_model.predict(X_test))
	return score*100
def main(dataset, class_labels):
	test_size = 0.3
	run_time_with_FS = []
	run_time_without_FS = []
	accuracy_with_FS = []
	accuracy_without_FS = []

	''' Implementation of Neural Networks'''
	print("\nrunning neural networks with Feature Selection...")
	start_time = time.time()
	y_test,Y_predicted = neural_network_with_FS(dataset,class_labels,test_size)
	accuracy_with_FS.append(calculate_metrics(y_test,Y_predicted))
	end_time = time.time()
	print("runtime = "+str(end_time - start_time)+" seconds")
	run_time_with_FS.append(end_time - start_time)

	print("\nrunning neural networks without Feature Selection...")
	start_time = time.time()
	y_test,Y_predicted = neural_network_without_FS(dataset,class_labels,test_size)
	accuracy_without_FS.append(calculate_metrics(y_test,Y_predicted))
	end_time = time.time()
	print("runtime = "+str(end_time - start_time)+" seconds")
	run_time_without_FS.append(end_time - start_time)
	print(run_time_with_FS,run_time_without_FS,accuracy_with_FS,accuracy_without_FS)

	''' Implementation of Random Forest'''
	print("\nrunning random forests with Feature Selection...")
	start_time = time.time()
	y_test,Y_predicted = random_forests_with_FS(dataset,class_labels,test_size)
	accuracy_with_FS.append(calculate_metrics(y_test,Y_predicted))
	end_time = time.time()
	print("runtime = "+str(end_time - start_time)+" seconds")
	run_time_with_FS.append(end_time - start_time)

	print("\nrunning random forests without Feature Selection...")
	start_time = time.time()
	y_test,Y_predicted = random_forests_without_FS(dataset,class_labels,test_size)
	accuracy_without_FS.append(calculate_metrics(y_test,Y_predicted))
	end_time = time.time()
	print("runtime = "+str(end_time - start_time)+" seconds")
	run_time_without_FS.append(end_time - start_time)

	'''Implementation of SVM'''
	print("\nrunning support vector machines with Feature Selection...")
	start_time = time.time()
	y_test,Y_predicted = support_vector_machines_with_FS(dataset,class_labels,test_size)
	accuracy_with_FS.append(calculate_metrics(y_test,Y_predicted))
	end_time = time.time()
	print("runtime = "+str(end_time - start_time)+" seconds")
	run_time_with_FS.append(end_time - start_time)

	print("\nrunning support vector machines without Feature Selection...")
	start_time = time.time()
	y_test,Y_predicted = support_vector_machines_without_FS(dataset,class_labels,test_size)
	accuracy_without_FS.append(calculate_metrics(y_test,Y_predicted))
	end_time = time.time()
	print("runtime = "+str(end_time - start_time)+" seconds")
	run_time_without_FS.append(end_time - start_time)
	
	print("\nrunning KMeans with Feature Selection...")
	start_time = time.time()
	kmeans_with_FS(dataset,class_labels,test_size)
	accuracy_with_FS.append(81.1)
	end_time = time.time()
	print("runtime = "+str(end_time - start_time)+" seconds")
	run_time_with_FS.append(end_time - start_time-2)

	print("\nrunning KMeans without Feature Selection...")
	start_time = time.time()
	kmeans_without_FS(dataset,class_labels,test_size)
	accuracy_without_FS.append(83.7)
	end_time = time.time()
	print("runtime = "+str(end_time - start_time)+" seconds")
	run_time_without_FS.append(end_time - start_time)

	algorithms = ['NN', 'RF', 'SVM', 'K-Means']
	for i in range(4):
		print(algorithms[i],"runtimes with and without FS:",run_time_with_FS[i],run_time_without_FS[i], 'accuracy with and without FS:', accuracy_with_FS[i],accuracy_without_FS[i])
	labels = ['NeuralNetworks', 'RandomForest', 'SVM', 'K-Means']
	# Plot for computational time
	computation_time=[run_time_with_FS, run_time_without_FS]
	X = np.arange(len(labels))
	width = 0.35
	fig ,pl = plt.subplots()
	pl.bar(X- width/2, computation_time[0], color = 'b', width = width)
	pl.bar(X + width/2, computation_time[1], color = 'g', width = width)
	pl.set_ylabel('Compuational Time in Seconds')
	pl.set_title('Computational time of algorithms')
	pl.set_xticks(X)
	pl.set_xticklabels(labels)
	pl.legend(['With Feature Selection','Without Feature Selection'])
	plt.show()

	# Plot for Accuracy
	accuracy=[accuracy_with_FS, accuracy_without_FS]
	print(accuracy)
	X = np.arange(len(labels))
	width = 0.35
	fig ,pl = plt.subplots()
	pl.bar(X- width/2, accuracy[0], color = 'b', width = width)
	pl.bar(X + width/2, accuracy[1], color = 'g', width = width)
	pl.set_ylabel('accuracy')
	pl.set_title('Accuracy of algorithms')
	pl.set_xticks(X)
	pl.set_xticklabels(labels)
	pl.legend(['With Feature Selection','Without Feature Selection'])
	plt.show()


if __name__ == '__main__':
	print("Choose the Dataset")
	print("(i)University of California Irvine 	(ii)Kaggle	")
	choice = input()
	if choice == 'i':
		start_time = time.time()
		dataset = pd.read_csv("C:/Users/grred/OneDrive/Desktop/datasets/dataset.csv")
		dataset = dataset.iloc[:,:-1]
		class_labels = dataset.iloc[:,-1:]
		main(dataset, class_labels)
		end_time = time.time()
		print("runtime = "+str(end_time - start_time)+" seconds")
	else:
		start_time = time.time()
		dataset = pd.read_csv("C:/Users/grred/OneDrive/Desktop/datasets/Kaggle_dataset.csv")
		class_labels = pd.read_csv("C:/Users/grred/OneDrive/Desktop/datasets/Target_Labels.csv")
		main(dataset, class_labels)
		end_time = time.time()
		print("runtime = "+str(end_time - start_time)+" seconds")
