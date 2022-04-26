#!C:\Users\grred\AppData\Local\Programs\Python\Python36\python.exe
import sys
import time
from sklearn import datasets
from sklearn.feature_selection import VarianceThreshold
import re
import sklearn
from sklearn.naive_bayes import MultinomialNB
from sklearn.metrics import accuracy_score
import features_extraction as fe
import site
import pandas as pd
import numpy as np
site.addsitedir('C:/Users/grred/AppData/Local/Programs/Python/Python36/lib/site-packages')
print("Content-Type: text/html\n\r\n")
from flask import Flask
from flask import jsonify
from flask import request

app = Flask(__name__)

def random_forests(dataset,class_labels,test_size):

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
	
	return model

@app.route('/main', methods=['GET', 'POST'])
def main():
	url = request.args.get('url')
	print(f"url is {url}")
	#url = "111111111111111111111111111111111111111111111111111111111111.com/"
	dataset = pd.read_csv("C:/Users/grred/OneDrive/Desktop/datasets/modified_dataset.csv")
	dataset = dataset.iloc[1:, :-1]
	class_labels = dataset.iloc[:, -1:]
	start_time = time.time()
	model = random_forests(dataset,class_labels,0.3)
	return_features = fe.main(url) 
	y_test = np.array(return_features)
	
	end_time = time.time()
	print("runtime = "+str(end_time - start_time)+" seconds")
	result= {
		'url' : url,
		'feature_string' : ''.join([str(i) for i in return_features]) ,
		'run_time' : str(end_time - start_time),
		'result_label' : str(model.predict(y_test.reshape(1,-1))[0])
	}
	return jsonify(code = 0 , msg = result)
	#return jsonify(code = 0 , msg = "feature string is: "+''.join([str(i) for i in return_features]) + ",  run time is: "+str(end_time - start_time)+" and class variable is: "+str(model.predict(y_test.reshape(1,-1))[0])+" "+url)
if __name__ == '__main__':
   app.run(port=5000, debug = True)
	