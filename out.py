from tensorflow.contrib.layers import fully_connected
import tensorflow as tf
import pandas as pd
import numpy as np
from sklearn.preprocessing import Imputer
from sklearn import cross_validation


'''Reading and preparing the data'''
data=pd.read_csv('Data4.csv')
species= list(data['Class'].unique())
data['One-hot'] = data['Class'].map(lambda x: np.eye(len(species))[species.index(x)] )

#shuffling the by default sorted data
data=data.iloc[np.random.permutation(len(data))]
data=data.reset_index(drop=True)

#train-test splitting  :n_test=100 is taken
keys=data.columns.values.tolist()
keys.pop()
keys.pop()
data.drop('Class',axis=1,inplace=True)
iris_matrix = pd.DataFrame.as_matrix(data[keys])
X = iris_matrix
X=Imputer().fit_transform(X)
Y = data['One-hot']
validation_size=0.20
seed=12
X_train, X_validation, Y_train, Y_validation = cross_validation.train_test_split(X, Y, test_size=validation_size, random_state=seed)



'''preprocessing completed'''

'''defining the RNN Network'''

n_steps = 8		#INTUITION: Since there are 8 threads,the whole of each thread individually may be considered as an input at each time step
n_inputs = 6	        #The 6 characterestics present for each thread				
n_neurons = 87		#Number of neurons in each layer	
n_outputs = 4		# 4 way classification
n_layers=3		# Number of layers
learning_rate = 0.0001

x= tf.placeholder(tf.float32, [None, n_steps, n_inputs])
y = tf.placeholder(tf.int32, [None,n_outputs])

basic_cell = tf.nn.rnn_cell.BasicLSTMCell(num_units=n_neurons)                            #creating a single LSTM Cell
multi_layer_cell=tf.nn.rnn_cell.MultiRNNCell([basic_cell]*n_layers)			  #creating 'n_layers' layers of the basic cell	
outputs, states = tf.nn.dynamic_rnn(multi_layer_cell, x, dtype=tf.float32)		  #getting the output sequence for input X
outputs=tf.unstack(tf.transpose(outputs,perm=[1,0,2]))			#this and the next line takes the last output of the output seq
in2full=outputs[-1]

logits = fully_connected(in2full, n_outputs, activation_fn=None)     	 #getting the logits(shape=[batch_size,3]) based on input X
cross_entropy=tf.nn.softmax_cross_entropy_with_logits(logits=logits,labels=y)    			#cross entropy error is takes as the error
loss=tf.reduce_mean(cross_entropy)
optimizer=tf.train.GradientDescentOptimizer(0.0001).minimize(cross_entropy)   #Gradient Descent to minimize the entropy,alpha=0.0001
correct=tf.equal(tf.argmax(logits,1),tf.argmax(y,1))
accuracy=tf.reduce_mean(tf.cast(correct,tf.float32)) 

 
'''Running the Session'''	
init = tf.global_variables_initializer()
n_epochs = 5000
with tf.Session() as sess:
    init.run()
    for epoch in range(n_epochs):
    	x_batch=X_train
    	x_batch=x_batch.reshape(-1,n_steps,n_inputs)
      	x_test=X_validation
    	x_test=x_test.reshape(-1,n_steps,n_inputs)
    	y_batch=[t for t in Y_train]
    	y_test=[t for t in Y_validation]
        sess.run(optimizer, feed_dict={x: x_batch, y: y_batch})
        acc_train = accuracy.eval(feed_dict={x: x_batch, y: y_batch})
        acc_test = accuracy.eval(feed_dict={x: x_test, y: y_test})
    	print(epoch, "Train accuracy:", acc_train, "Test accuracy:", acc_test)
acc_test = accuracy.eval(feed_dict={x: x_test, y: y_test})
print(epoch, "Train accuracy:", acc_train, "Test accuracy:", acc_test)
    	
    	
