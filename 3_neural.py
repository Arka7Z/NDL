import tensorflow as tf
import pandas as pd
import numpy as np
from sklearn.preprocessing import Imputer
from sklearn import cross_validation

data=pd.read_csv('/home/amanpurwar/myData.csv')
K=3
n_features=56

x=tf.placeholder(tf.float32,shape=[None,n_features])       #placeholders for feeding in the X_test and Y_test while running the session
y=tf.placeholder(tf.float32,shape=[None,K])
keep_prob=tf.placeholder(tf.float32)               #placeholder for the dropout probability of neurons

#converting the data frame  into one hot encoding
species= list(data['Class'].unique())
data['One-hot'] = data['Class'].map(lambda x: np.eye(len(species))[species.index(x)] )

keys=data.columns.values.tolist()  
keys.pop()			  
keys.pop()
#data[keys]=(data[keys]-data[keys].mean())/(data[keys].std())
data.drop('Class',axis=1,inplace=True)		           

#shuffling the by default sorted data
data=data.iloc[np.random.permutation(len(data))]
data=data.reset_index(drop=True)

iris_matrix = pd.DataFrame.as_matrix(data[keys])
X = iris_matrix
X=Imputer().fit_transform(X)
#np.nan_to_num(X)
Y = data['One-hot']
validation_size=0.20
seed=12
X_train, X_validation, Y_train, Y_validation = cross_validation.train_test_split(X, Y, test_size=validation_size, random_state=seed)

#x_test=data.ix[len(data)-40:,[keys.values]]
#y_test=data.ix[len(data)-100:,['Class','One-hot']]


#functions to return random weight and bias variables
def weight_var(shape):
	init_state=tf.truncated_normal(shape,stddev=0.1)
	return tf.Variable(init_state)
	
def bias_var(shape):
	init_state=tf.truncated_normal(shape,stddev=0.1)
	return tf.Variable(init_state)


#feedforward the input	
def runANN(x):
	n_hl1=90   #no of neurons in first layer
	n_hl2=95 #no of neurons in second layer
	n_hl3=90    #no of neuron in third layer
	n_hl4=80
	n_hl5=75

	n_class=K    #3 way classification
	
	
	#relu is used as the activation function
	
	weight1=weight_var([n_features,n_hl1])
	bias1=bias_var([n_hl1])
	a_2=tf.nn.relu(tf.add(tf.matmul(x,weight1),bias1))   		   #layer 1:computes ReLu(W1*X+b1) which is assigned to a_2
	
	weight2=weight_var([n_hl1,n_hl2])
	bias2=bias_var([n_hl2])
	a_3=tf.nn.relu(tf.add(tf.matmul(a_2,weight2),bias2)) 		   #layer 2:computes ReLu(W2*a_2+b2) which is assigned to a_3
	
	weight3=weight_var([n_hl2,n_hl3])
	bias3=bias_var([n_hl3])
	a_4=tf.nn.relu(tf.add(tf.matmul(a_3,weight3),bias3)) 		   #layer 3:computes ReLu(W3*a_3+b3) which is assigned to a_4
	
	weight4=weight_var([n_hl3,n_hl4])
	bias4=bias_var([n_hl4])
	a_5=tf.nn.relu(tf.add(tf.matmul(a_4,weight4),bias4)) 		   #layer 2:computes ReLu(W2*a_2+b2) which is assigned to a_3
	
	a_5=tf.nn.dropout(a_5,keep_prob) 
	
	weight_final=weight_var([n_hl4,n_class])
	bias_final=bias_var([n_class])
	y_pred=tf.nn.relu(tf.add(tf.matmul(a_5,weight_final),bias_final))  #calculates the final predicted scores for each class
	
	return y_pred
	
def minimize_loss():
	y_pred=runANN(x)
	cross_entropy=tf.nn.softmax_cross_entropy_with_logits(y_pred,y)    #cross entropy error is takes as the error
	loss=tf.reduce_mean(cross_entropy)
	optimizer=tf.train.GradientDescentOptimizer(0.0001).minimize(cross_entropy)   #Gradient Descent to minimize the entropy,alpha=0.0001
	
	correct=tf.equal(tf.argmax(y_pred,1),tf.argmax(y,1))
	accuracy=tf.reduce_mean(tf.cast(correct,tf.float32))               #defining correctness and accuracy
	
	with tf.Session() as sess:
		sess.run(tf.global_variables_initializer())
		n_iter=20000      					   #number of iterations
		for i in range(n_iter):
			#train=trainSet 		           #batch size 50
			sess.run(optimizer,feed_dict={keep_prob:0.5,x:[t for t in X_train],y:[t for t in Y_train]})
			acc_train = accuracy.eval(feed_dict={keep_prob:0.5,x:[t for t in X_train],y:[t for t in Y_train]})
			acc=sess.run(accuracy,feed_dict={keep_prob:1.0,x:[t for t in X_validation],y:[t for t in Y_validation]})
			print(i," ",acc_train," ",acc)
			
		acc=sess.run(accuracy,feed_dict={keep_prob:1.0,x:[t for t in X_validation],y:[t for t in Y_validation]})
		print("Accuracy is:")
		print(acc)
	
	
minimize_loss()



